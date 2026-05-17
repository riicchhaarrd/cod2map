'use strict';

const mapSelect = document.getElementById('mapSelect');
const consoleEl = document.getElementById('console');
const sourceStatus = document.getElementById('sourceStatus');
const runStatus = document.getElementById('runStatus');
const downloads = document.getElementById('downloads');
const filePicker = document.getElementById('filePicker');

const OPFS_ROOT = 'cod2map-vfs';
const DEFAULT_MOUNT_ROOT = '/vfs/game';

let currentSource = null;
let latestRunId = 0;

function log(line) {
  consoleEl.textContent += `${line}\n`;
  consoleEl.scrollTop = consoleEl.scrollHeight;
}

function setStatus(element, message) {
  element.textContent = message;
}

function normalizePath(path) {
  return String(path || '')
    .replace(/\\/g, '/')
    .replace(/^\/+/, '')
    .replace(/\/+/g, '/')
    .replace(/\/\.\//g, '/')
    .replace(/(^|\/)\.($|\/)/g, '/')
    .replace(/\/$/, '');
}

function dirname(path) {
  const i = path.lastIndexOf('/');
  return i === -1 ? '' : path.slice(0, i);
}

function safeRelativePath(path) {
  const rel = normalizePath(path);
  if (!rel || rel === '.' || rel.includes('..')) {
    throw new Error(`Unsafe relative path: ${path}`);
  }
  return rel;
}

function sanitizeMountName(name) {
  const clean = String(name || 'game')
    .normalize('NFKD')
    .replace(/[^a-zA-Z0-9._-]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .slice(0, 48);
  return clean || 'game';
}

function mountRootForLabel(label) {
  return `/vfs/${sanitizeMountName(label)}`;
}

function pathSegments(path) {
  return normalizePath(path).toLowerCase().split('/').filter(Boolean);
}

function hasPathSegment(path, segment) {
  return pathSegments(path).includes(segment.toLowerCase());
}

function isAutosaveMap(path) {
  const name = normalizePath(path).split('/').pop().toLowerCase();
  return name === 'autosave.map' || name.endsWith('_autosave.map') || name.endsWith('.autosave.map');
}

function inferMapChoice(path) {
  const rel = normalizePath(path);
  const lower = rel.toLowerCase();
  const segments = pathSegments(rel);

  if (!lower.endsWith('.map')) return null;
  if (isAutosaveMap(rel)) return null;
  if (segments.includes('bin') || segments.includes('collmaps')) return null;
  if (segments.includes('prefabs')) return null;

  if (lower.startsWith('map_source/')) {
    const sourceRel = rel.slice('map_source/'.length);
    if (!sourceRel || sourceRel.toLowerCase().startsWith('prefabs/')) return null;

    const compileRel = sourceRel.toLowerCase().startsWith('maps/')
      ? `main/${sourceRel}`
      : `main/maps/${sourceRel}`;
    return {
      sourcePath: rel,
      compilePath: compileRel,
      loadFromPath: rel,
      label: `${rel} → ${compileRel}`
    };
  }

  if (lower.startsWith('devraw/')) return null;

  if (hasPathSegment(rel, 'maps')) {
    return {
      sourcePath: rel,
      compilePath: rel,
      loadFromPath: '',
      label: rel
    };
  }

  return null;
}

function collectMapChoices(paths) {
  const bySource = new Map();
  for (const path of paths) {
    const choice = inferMapChoice(path);
    if (!choice) continue;
    if (!bySource.has(choice.sourcePath)) bySource.set(choice.sourcePath, choice);
  }
  return [...bySource.values()].sort((a, b) => a.label.localeCompare(b.label));
}

function analyzeSourceLayout(paths) {
  const lower = new Set(paths.map(p => normalizePath(p).toLowerCase()));
  const hasMainIwd = [...lower].some(p => /^main\/[^/]+\.iwd$/.test(p));
  const hasDefaultCfg = lower.has('main/default.cfg') || lower.has('main/default_localize.cfg');
  const hasMainDir = [...lower].some(p => p.startsWith('main/'));
  const warnings = [];

  if (!hasMainDir) {
    warnings.push('no main/ folder detected; select the CoD2 install root, not bin/ or map_source/');
  } else if (!hasMainIwd && !hasDefaultCfg) {
    warnings.push('no main/*.iwd or main/default.cfg detected; the selected folder may not be a complete CoD2 install');
  }

  return warnings;
}

function setMapOptions(paths) {
  mapSelect.textContent = '';
  const choices = collectMapChoices(paths);

  for (const choice of choices) {
    const opt = document.createElement('option');
    opt.value = choice.sourcePath;
    opt.dataset.sourcePath = choice.sourcePath;
    opt.dataset.compilePath = choice.compilePath;
    opt.dataset.loadFromPath = choice.loadFromPath;
    opt.textContent = choice.label;
    mapSelect.appendChild(opt);
  }

  const warnings = currentSource ? analyzeSourceLayout(paths) : [];
  const warningText = warnings.length ? ` Warning: ${warnings.join('; ')}.` : '';
  const sourceText = currentSource
    ? `${currentSource.label}: ${currentSource.entries.length} file reference(s), ${choices.length} compilable map source(s).${warningText}`
    : 'No files loaded.';
  setStatus(sourceStatus, choices.length ? sourceText : `${sourceText} No compilable map sources were found.`);
}

async function* walkDirectoryHandles(handle, prefix = '') {
  for await (const [name, child] of handle.entries()) {
    const rel = safeRelativePath(prefix ? `${prefix}/${name}` : name);
    if (child.kind === 'directory') {
      yield* walkDirectoryHandles(child, rel);
    } else if (child.kind === 'file') {
      yield { path: rel, handle: child };
    }
  }
}

async function scanDirectorySource(handle) {
  const entries = [];
  let count = 0;
  for await (const entry of walkDirectoryHandles(handle)) {
    entries.push(entry);
    count++;
    if (count % 500 === 0) setStatus(sourceStatus, `Indexed ${count} file reference(s)...`);
  }
  currentSource = {
    type: 'directory',
    label: handle.name ? `Directory handle VFS: ${handle.name}` : 'Directory handle VFS',
    mountRoot: mountRootForLabel(handle.name || 'game'),
    entries
  };
  setMapOptions(entries.map(e => e.path));
}

async function opfsRoot() {
  if (!navigator.storage || !navigator.storage.getDirectory) {
    throw new Error('OPFS is unavailable in this browser.');
  }
  return navigator.storage.getDirectory();
}

async function opfsGameRoot(create = true) {
  const root = await opfsRoot();
  return root.getDirectoryHandle(OPFS_ROOT, { create });
}

async function ensureOpfsDir(root, path) {
  let dir = root;
  for (const part of path.split('/').filter(Boolean)) {
    dir = await dir.getDirectoryHandle(part, { create: true });
  }
  return dir;
}

async function writeOpfsFile(path, file) {
  const rel = safeRelativePath(path);
  const root = await opfsGameRoot(true);
  const parent = await ensureOpfsDir(root, dirname(rel));
  const name = rel.split('/').pop();
  const fh = await parent.getFileHandle(name, { create: true });
  const writable = await fh.createWritable();
  await writable.write(file);
  await writable.close();
}

async function* walkOpfs(dir, prefix = '') {
  for await (const [name, handle] of dir.entries()) {
    const rel = safeRelativePath(prefix ? `${prefix}/${name}` : name);
    if (handle.kind === 'directory') {
      yield* walkOpfs(handle, rel);
    } else if (handle.kind === 'file') {
      yield { path: rel, handle };
    }
  }
}

async function scanOpfsSource() {
  let root;
  try {
    root = await opfsGameRoot(false);
  } catch (e) {
    currentSource = { type: 'opfs', label: 'OPFS staged VFS', mountRoot: DEFAULT_MOUNT_ROOT, entries: [] };
    setMapOptions([]);
    return;
  }

  const entries = [];
  for await (const entry of walkOpfs(root)) entries.push(entry);
  currentSource = { type: 'opfs', label: 'OPFS staged VFS', mountRoot: DEFAULT_MOUNT_ROOT, entries };
  setMapOptions(entries.map(e => e.path));
}

async function clearOpfsGameRoot() {
  const root = await opfsRoot();
  try {
    await root.removeEntry(OPFS_ROOT, { recursive: true });
  } catch (e) {
    // Nothing to clear.
  }
}

async function collectFilesForRun() {
  if (!currentSource) throw new Error('No source is loaded.');

  const files = [];
  const total = currentSource.entries.length;

  for (let i = 0; i < total; i++) {
    const entry = currentSource.entries[i];
    let file;

    if (currentSource.type === 'directory') {
      file = await entry.handle.getFile();
    } else if (currentSource.type === 'opfs') {
      file = await entry.handle.getFile();
    } else {
      throw new Error(`Unknown source type: ${currentSource.type}`);
    }

    files.push({ path: entry.path, file });
    if ((i + 1) % 500 === 0) {
      setStatus(runStatus, `Prepared ${i + 1}/${total} lazy file reference(s)...`);
    }
  }

  return files;
}

function addDownload(path, arrayBuffer) {
  const blob = new Blob([arrayBuffer], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = path.split('/').pop();
  a.textContent = `Download ${a.download}`;
  downloads.appendChild(a);
}

async function runSelectedMap() {
  downloads.textContent = '';
  consoleEl.textContent = '';

  const selectedOption = mapSelect.selectedOptions[0];
  const selectedMap = selectedOption?.dataset.sourcePath || mapSelect.value;
  const compileMap = selectedOption?.dataset.compilePath || selectedMap;
  const loadFromPath = selectedOption?.dataset.loadFromPath || '';
  if (!selectedMap || !compileMap) {
    setStatus(runStatus, 'No map selected.');
    return;
  }

  if (!currentSource || !currentSource.entries.length) {
    setStatus(runStatus, 'No VFS source is loaded.');
    return;
  }

  setStatus(runStatus, 'Preparing lazy VFS file references...');
  let files;
  try {
    files = await collectFilesForRun();
  } catch (error) {
    setStatus(runStatus, error.message || String(error));
    return;
  }

  latestRunId++;
  const runId = latestRunId;
  const worker = new Worker('runner.worker.js');

  worker.addEventListener('message', event => {
    const message = event.data || {};
    if (message.runId !== runId) return;

    if (message.type === 'log') {
      log(message.line);
    } else if (message.type === 'status') {
      setStatus(runStatus, message.message);
    } else if (message.type === 'download') {
      addDownload(message.path, message.data);
    } else if (message.type === 'done') {
      setStatus(runStatus, message.message);
      worker.terminate();
    } else if (message.type === 'error') {
      log(message.message);
      setStatus(runStatus, 'Browser run failed. Check console output.');
      worker.terminate();
    }
  });

  worker.addEventListener('error', event => {
    log(`${event.message} (${event.filename}:${event.lineno})`);
    setStatus(runStatus, 'Worker failed. Check console output.');
    worker.terminate();
  });

  worker.postMessage({
    type: 'run',
    runId,
    selectedMap,
    compileMap,
    loadFromPath,
    platformPc: document.getElementById('platformPc').checked,
    mountRoot: currentSource.mountRoot || DEFAULT_MOUNT_ROOT,
    files
  });
}

document.getElementById('chooseFolder').addEventListener('click', async () => {
  if (!window.showDirectoryPicker) {
    setStatus(sourceStatus, 'Directory picker is unavailable in this browser. Use OPFS staging with a map/assets subset.');
    return;
  }

  try {
    const handle = await window.showDirectoryPicker({ mode: 'read' });
    setStatus(sourceStatus, 'Indexing folder. File contents are not copied.');
    await scanDirectorySource(handle);
  } catch (error) {
    if (error && error.name === 'AbortError') return;
    setStatus(sourceStatus, error.message || String(error));
  }
});

document.getElementById('stageFiles').addEventListener('click', () => {
  filePicker.click();
});

filePicker.addEventListener('change', async () => {
  const files = Array.from(filePicker.files || []);
  if (!files.length) return;

  try {
    setStatus(sourceStatus, `Staging ${files.length} selected file(s) into OPFS...`);
    for (let i = 0; i < files.length; i++) {
      const file = files[i];
      const rel = safeRelativePath(file.webkitRelativePath || file.name);
      await writeOpfsFile(rel, file);
      if ((i + 1) % 200 === 0) {
        setStatus(sourceStatus, `Staged ${i + 1}/${files.length} selected file(s) into OPFS...`);
      }
    }
    await scanOpfsSource();
  } catch (error) {
    setStatus(sourceStatus, error.message || String(error));
  } finally {
    filePicker.value = '';
  }
});

document.getElementById('clearOpfs').addEventListener('click', async () => {
  try {
    await clearOpfsGameRoot();
    if (currentSource?.type === 'opfs') currentSource = null;
    mapSelect.textContent = '';
    setStatus(sourceStatus, 'OPFS staged VFS cleared.');
  } catch (error) {
    setStatus(sourceStatus, error.message || String(error));
  }
});

document.getElementById('refreshMaps').addEventListener('click', async () => {
  if (currentSource?.type === 'opfs') {
    await scanOpfsSource();
  } else if (currentSource?.entries) {
    setMapOptions(currentSource.entries.map(e => e.path));
  } else {
    setStatus(sourceStatus, 'No source is loaded.');
  }
});

document.getElementById('runMap').addEventListener('click', runSelectedMap);
