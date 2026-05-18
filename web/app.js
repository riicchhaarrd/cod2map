'use strict';

const $ = id => document.getElementById(id);

const mapSelect = $('mapSelect');
const consoleEl = $('console');
const sourceStatus = $('sourceStatus');
const runStatus = $('runStatus');
const downloads = $('downloads');
const filePicker = $('filePicker');
const mapFilter = $('mapFilter');
const runButton = $('runMap');
const txtTreePath = $('txtTreePath');
const restoreButton = $('restoreFolder');
const mapSubFolder = $('mapSubFolder');
const myMapToggle = $('myMapToggle');
const tabAllMaps = $('tabAllMaps');
const tabMyMaps = $('tabMyMaps');
const summaryFiles = $('summaryFiles');
const summaryMaps = $('summaryMaps');
const summaryOutputs = $('summaryOutputs');
const inlineConsoleFallback = $('inlineConsoleFallback');

const OPFS_ROOT = 'cod2map-vfs';
const DEFAULT_MOUNT_ROOT = '/vfs/game';
const SETTINGS_KEY = 'cod2compiletools.settings.v1';
const SOURCE_KEY = 'cod2compiletools.source.v1';
const DB_NAME = 'cod2compiletools';
const DB_STORE = 'handles';
const DB_HANDLE_KEY = 'cod2-root';

let currentSource = null;
let latestRunId = 0;
let currentMapChoices = [];
let activeMapTab = 'all';
let outputCount = 0;
let logBuffer = '';
let logWindow = null;
let consoleAutoCloseTimer = null;
let settings = loadSettings();
let savedSource = loadSavedSource();

function loadSettings() {
  try {
    return JSON.parse(localStorage.getItem(SETTINGS_KEY) || '{}') || {};
  } catch (_) {
    return {};
  }
}

function saveSettings() {
  localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
}

function loadSavedSource() {
  try {
    return JSON.parse(localStorage.getItem(SOURCE_KEY) || '{}') || {};
  } catch (_) {
    return {};
  }
}

function saveSavedSource() {
  localStorage.setItem(SOURCE_KEY, JSON.stringify(savedSource));
}

function openDb() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, 1);
    request.onupgradeneeded = () => {
      request.result.createObjectStore(DB_STORE);
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error || new Error('Could not open IndexedDB.'));
  });
}

async function putDirectoryHandle(handle) {
  const db = await openDb();
  await new Promise((resolve, reject) => {
    const tx = db.transaction(DB_STORE, 'readwrite');
    tx.objectStore(DB_STORE).put(handle, DB_HANDLE_KEY);
    tx.oncomplete = resolve;
    tx.onerror = () => reject(tx.error || new Error('Could not save directory handle.'));
  });
  db.close();
}

async function getDirectoryHandle() {
  const db = await openDb();
  const handle = await new Promise((resolve, reject) => {
    const tx = db.transaction(DB_STORE, 'readonly');
    const request = tx.objectStore(DB_STORE).get(DB_HANDLE_KEY);
    request.onsuccess = () => resolve(request.result || null);
    request.onerror = () => reject(request.error || new Error('Could not read saved directory handle.'));
  });
  db.close();
  return handle;
}

function consoleHtml() {
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>cod2map console</title>
<style>
html, body { margin: 0; height: 100%; background: #000; color: #c0c0c0; }
body { font: 13px/1.35 Consolas, "Lucida Console", "Courier New", monospace; }
.bar { height: 26px; line-height: 26px; padding: 0 8px; background: #111; border-bottom: 1px solid #333; color: #fff; user-select: none; }
#cmd-output { box-sizing: border-box; width: 100%; height: calc(100% - 26px); margin: 0; padding: 8px; overflow: auto; white-space: pre-wrap; word-break: break-word; }
</style>
</head>
<body>
<div class="bar">cod2map.exe</div>
<pre id="cmd-output"></pre>
</body>
</html>`;
}

function renderConsoleWindow() {
  if (!logWindow || logWindow.closed) return;
  const out = logWindow.document && logWindow.document.getElementById('cmd-output');
  if (!out) return;
  out.textContent = logBuffer;
  out.scrollTop = out.scrollHeight;
}

function showInlineLog(open = false) {
  if (!inlineConsoleFallback) return;
  inlineConsoleFallback.hidden = false;
  if (open && 'open' in inlineConsoleFallback) inlineConsoleFallback.open = true;
}

function cancelConsoleAutoClose() {
  if (consoleAutoCloseTimer) {
    clearTimeout(consoleAutoCloseTimer);
    consoleAutoCloseTimer = null;
  }
}

function scheduleConsoleAutoClose(delayMs = 1500) {
  cancelConsoleAutoClose();
  const win = logWindow;
  if (!win || win.closed) return;
  consoleAutoCloseTimer = setTimeout(() => {
    consoleAutoCloseTimer = null;
    if (logWindow !== win || win.closed) return;
    try { win.close(); } catch (_) {}
    if (logWindow === win) logWindow = null;
  }, delayMs);
}

function openConsoleWindow(focus = true) {
  if (focus) cancelConsoleAutoClose();
  if (logWindow && !logWindow.closed) {
    renderConsoleWindow();
    if (focus) logWindow.focus();
    return true;
  }

  const win = window.open('', 'cod2map-console', 'width=960,height=540,resizable=yes,scrollbars=yes');
  if (!win) {
    showInlineLog(true);
    return false;
  }

  logWindow = win;
  win.document.open();
  win.document.write(consoleHtml());
  win.document.close();
  try {
    win.addEventListener('beforeunload', () => { logWindow = null; });
  } catch (_) {}
  renderConsoleWindow();
  if (focus) win.focus();
  return true;
}

function setLogText(text) {
  logBuffer = String(text || '');
  if (logBuffer) showInlineLog(false);
  consoleEl.textContent = logBuffer;
  consoleEl.scrollTop = consoleEl.scrollHeight;
  renderConsoleWindow();
}

function log(line) {
  logBuffer += `${line}\n`;
  showInlineLog(false);
  consoleEl.textContent = logBuffer;
  consoleEl.scrollTop = consoleEl.scrollHeight;
  renderConsoleWindow();
}

function setStatus(element, message) {
  if (element) element.textContent = message;
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

function displayPath(path) {
  return normalizePath(path).replace(/\//g, '\\');
}

function basename(path) {
  return normalizePath(path).split('/').pop() || path;
}

function inferGameModeFromPath(path) {
  const rel = normalizePath(path);
  const lower = rel.toLowerCase();
  const name = basename(rel).toLowerCase();
  const segments = pathSegments(rel);

  if (segments.includes('mp')) return 'mp';
  if (lower.includes('/maps/mp/')) return 'mp';
  if (lower.includes('/map_source/mp/')) return 'mp';
  if (name.startsWith('mp_')) return 'mp';
  return 'sp';
}

function buildCompilePathForMapSource(sourceRel) {
  const rel = normalizePath(sourceRel);
  const lower = rel.toLowerCase();
  const mode = inferGameModeFromPath(rel);

  if (lower.startsWith('maps/mp/')) return { compilePath: `main/${rel}`, gameMode: 'mp' };

  if (lower.startsWith('maps/')) {
    const rest = rel.slice('maps/'.length);
    if (inferGameModeFromPath(rest) === 'mp') {
      return { compilePath: `main/maps/mp/${rest}`, gameMode: 'mp' };
    }
    return { compilePath: `main/maps/${rest}`, gameMode: mode };
  }

  if (lower.startsWith('mp/')) return { compilePath: `main/maps/${rel}`, gameMode: 'mp' };
  if (mode === 'mp') return { compilePath: `main/maps/mp/${rel}`, gameMode: 'mp' };
  return { compilePath: `main/maps/${rel}`, gameMode: 'sp' };
}

function mapLabelPrefix(gameMode) {
  return gameMode === 'mp' ? '[MP]' : '[SP]';
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

    const { compilePath, gameMode } = buildCompilePathForMapSource(sourceRel);
    return {
      sourcePath: rel,
      compilePath,
      loadFromPath: rel,
      gameMode,
      label: `${mapLabelPrefix(gameMode)} ${displayPath(rel)}`,
      shortName: basename(rel)
    };
  }

  if (lower.startsWith('devraw/')) return null;

  if (hasPathSegment(rel, 'maps')) {
    const gameMode = inferGameModeFromPath(rel);
    return {
      sourcePath: rel,
      compilePath: rel,
      loadFromPath: '',
      gameMode,
      label: `${mapLabelPrefix(gameMode)} ${displayPath(rel)}`,
      shortName: basename(rel)
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
  const hasMainDir = [...lower].some(p => p.startsWith('main/'));
  const warnings = [];

  if (!hasMainDir) {
    warnings.push('not a CoD2 root: main folder missing');
  } else if (!hasMainIwd) {
    warnings.push('main/*.iwd not found');
  }

  return warnings;
}

function selectedMapChoice() {
  const opt = mapSelect.selectedOptions[0];
  if (!opt) return null;
  return {
    sourcePath: opt.dataset.sourcePath,
    compilePath: opt.dataset.compilePath,
    loadFromPath: opt.dataset.loadFromPath || '',
    gameMode: opt.dataset.gameMode || inferGameModeFromPath(opt.dataset.sourcePath || '')
  };
}

function mapFolderFilterPrefix() {
  const value = normalizePath(mapSubFolder?.value || '');
  return value ? value.toLowerCase() : '';
}

function isInMapFolder(choice) {
  const prefix = mapFolderFilterPrefix();
  if (!prefix || prefix === 'map_source') return true;
  return choice.sourcePath.toLowerCase().startsWith(`${prefix}/`);
}

function myMapsSet() {
  if (!Array.isArray(settings.mymaps)) settings.mymaps = [];
  return new Set(settings.mymaps);
}

function isMyMap(choice) {
  return myMapsSet().has(choice.sourcePath);
}

function setActiveTab(tab) {
  activeMapTab = tab;
  tabAllMaps.classList.toggle('active', tab === 'all');
  tabMyMaps.classList.toggle('active', tab === 'my');
  tabAllMaps.setAttribute('aria-selected', tab === 'all' ? 'true' : 'false');
  tabMyMaps.setAttribute('aria-selected', tab === 'my' ? 'true' : 'false');
  settings.mapstab = tab;
  saveSettings();
  renderMapOptions();
}

function updateMyMapButton() {
  const choice = selectedMapChoice();
  if (!choice) {
    myMapToggle.disabled = true;
    myMapToggle.textContent = "Add to 'My Maps'";
    return;
  }
  myMapToggle.disabled = false;
  myMapToggle.textContent = myMapsSet().has(choice.sourcePath) ? "Remove from 'My Maps'" : "Add to 'My Maps'";
}

function renderMapOptions() {
  const filter = normalizePath(mapFilter?.value || '').toLowerCase();
  const selected = mapSelect.value || settings.mapname || '';
  mapSelect.textContent = '';

  const visibleChoices = currentMapChoices.filter(choice => {
    if (!isInMapFolder(choice)) return false;
    if (activeMapTab === 'my' && !isMyMap(choice)) return false;
    if (!filter) return true;
    return choice.label.toLowerCase().includes(filter) || choice.shortName.toLowerCase().includes(filter);
  });

  for (const choice of visibleChoices) {
    const opt = document.createElement('option');
    opt.value = choice.sourcePath;
    opt.dataset.sourcePath = choice.sourcePath;
    opt.dataset.compilePath = choice.compilePath;
    opt.dataset.loadFromPath = choice.loadFromPath;
    opt.dataset.gameMode = choice.gameMode;
    opt.textContent = choice.label;
    mapSelect.appendChild(opt);
  }

  if ([...mapSelect.options].some(opt => opt.value === selected)) mapSelect.value = selected;
  if (!mapSelect.value && mapSelect.options.length) mapSelect.selectedIndex = 0;

  if (mapSelect.value) {
    settings.mapname = mapSelect.value;
    saveSettings();
  }
  updateMyMapButton();
}

function setMapOptions(paths) {
  currentMapChoices = collectMapChoices(paths);
  renderMapOptions();

  const warnings = currentSource ? analyzeSourceLayout(paths) : [];
  const warningText = warnings.length ? ` (${warnings.join('; ')})` : '';
  const sourceText = currentSource
    ? `${currentSource.label} · ${currentSource.entries.length} files · ${currentMapChoices.length} maps${warningText}`
    : 'No files loaded.';
  setStatus(sourceStatus, currentMapChoices.length ? sourceText : `${sourceText} No compilable maps found.`);
  updateSummary(paths.length, currentMapChoices.length);
}

function updateSummary(fileCount = null, mapCount = null) {
  if (summaryFiles) summaryFiles.textContent = String(fileCount ?? currentSource?.entries?.length ?? 0);
  if (summaryMaps) summaryMaps.textContent = String(mapCount ?? currentMapChoices.length ?? 0);
  if (summaryOutputs) summaryOutputs.textContent = String(outputCount);
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
    label: handle.name || 'CoD2 folder',
    mountRoot: mountRootForLabel(handle.name || 'game'),
    entries,
    rootHandle: handle
  };
  txtTreePath.value = handle.name || '';
  setMapOptions(entries.map(e => e.path));
}

async function requestDirectoryPermission(handle) {
  const opts = { mode: 'read' };
  if (await handle.queryPermission?.(opts) === 'granted') return true;
  return await handle.requestPermission?.(opts) === 'granted';
}

async function requestDirectoryWritePermission(handle) {
  const opts = { mode: 'readwrite' };
  if (await handle.queryPermission?.(opts) === 'granted') return true;
  return await handle.requestPermission?.(opts) === 'granted';
}

async function hasDirectoryWritePermission(handle) {
  const opts = { mode: 'readwrite' };
  return await handle.queryPermission?.(opts) === 'granted';
}

async function saveDirectorySource(handle) {
  await putDirectoryHandle(handle);
  savedSource = { type: 'directory', name: handle.name || 'CoD2 folder', savedAt: Date.now() };
  saveSavedSource();
  restoreButton.disabled = false;
  txtTreePath.value = savedSource.name;
}

async function restoreSavedDirectory(auto = false) {
  let handle;
  try {
    handle = await getDirectoryHandle();
  } catch (error) {
    if (!auto) setStatus(sourceStatus, error.message || String(error));
    return false;
  }

  if (!handle) return false;
  txtTreePath.value = savedSource.name || handle.name || '';
  restoreButton.disabled = false;

  const granted = auto
    ? await handle.queryPermission?.({ mode: 'read' }) === 'granted'
    : await requestDirectoryPermission(handle);
  if (!granted) {
    if (!auto) setStatus(sourceStatus, 'Permission is required to restore the saved CoD2 folder.');
    return false;
  }

  setStatus(sourceStatus, 'Restoring saved CoD2 folder...');
  await scanDirectorySource(handle);
  return true;
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
    currentSource = { type: 'opfs', label: 'OPFS staged files', mountRoot: DEFAULT_MOUNT_ROOT, entries: [] };
    setMapOptions([]);
    return;
  }

  const entries = [];
  for await (const entry of walkOpfs(root)) entries.push(entry);
  currentSource = { type: 'opfs', label: 'OPFS staged files', mountRoot: DEFAULT_MOUNT_ROOT, entries };
  txtTreePath.value = 'OPFS staged files';
  setMapOptions(entries.map(e => e.path));
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
      setStatus(runStatus, `Prepared ${i + 1}/${total} file reference(s)...`);
    }
  }

  return files;
}

function formatBytes(bytes) {
  if (!Number.isFinite(bytes)) return '';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MiB`;
}

function resetDownloads() {
  outputCount = 0;
  downloads.textContent = 'No generated files yet.';
  downloads.classList.add('empty');
  updateSummary();
}

function addDownload(path, arrayBuffer, relativePath = '') {
  if (downloads.classList.contains('empty')) {
    downloads.textContent = '';
    downloads.classList.remove('empty');
  }

  outputCount++;
  const blob = new Blob([arrayBuffer], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = path.split('/').pop();
  a.textContent = a.download;

  const meta = document.createElement('span');
  meta.className = 'output-meta';
  meta.textContent = relativePath
    ? `${formatBytes(arrayBuffer.byteLength)} · pending write to ${displayPath(relativePath)}`
    : formatBytes(arrayBuffer.byteLength);

  downloads.appendChild(a);
  downloads.appendChild(meta);
  updateSummary();
  return { link: a, meta };
}

async function ensureDirectoryHandle(rootHandle, relDir) {
  let dir = rootHandle;
  for (const part of normalizePath(relDir).split('/').filter(Boolean)) {
    dir = await dir.getDirectoryHandle(part, { create: true });
  }
  return dir;
}

async function writeGeneratedOutputToDirectory(relativePath, arrayBuffer) {
  if (!currentSource || currentSource.type !== 'directory' || !currentSource.rootHandle) {
    return null;
  }

  const rel = safeRelativePath(relativePath);
  if (!/\.(d3dbsp|d3dbsp_xenon|bsp|lin|prt|reg)$/i.test(rel)) {
    throw new Error(`Refusing to write unexpected output file: ${rel}`);
  }

  if (!(await hasDirectoryWritePermission(currentSource.rootHandle))) {
    throw new Error('Write permission was not granted before compile started.');
  }

  const dir = await ensureDirectoryHandle(currentSource.rootHandle, dirname(rel));
  const name = rel.split('/').pop();
  const fileHandle = await dir.getFileHandle(name, { create: true });
  const writable = await fileHandle.createWritable();
  try {
    await writable.write(new Blob([arrayBuffer], { type: 'application/octet-stream' }));
  } finally {
    await writable.close();
  }
  return rel;
}

function splitCommandLine(value) {
  return String(value || '').trim().split(/\s+/).filter(Boolean);
}

function bspOptionArgs() {
  const args = [];
  const addFlag = (id, flag) => { if ($(id)?.checked) args.push(flag); };
  const addValue = (checkId, textId, flag) => {
    if ($(checkId)?.checked) {
      args.push(flag);
      const value = $(textId)?.value?.trim();
      if (value) args.push(value);
    }
  };

  addFlag('chkOnlyEnts', '-onlyEnts');
  addValue('chkBlockSize', 'txtBlockSize', '-blockSize');
  addValue('chkSampleScale', 'txtSampleScale', '-sampleScale');
  args.push(...splitCommandLine($('txtBSPOptions')?.value));
  return args;
}

function visOptionArgs() {
  const args = [];
  // The original browser UI keeps the CoD2CompileTools layout, but the only
  // compatible Vis controls in this Wasm build are passed through custom BSP
  // options when running a pure vis command. Keep this intentionally minimal.
  return args;
}

function persistOptions() {
  const ids = [
    'chkBSP', 'chkVis', 'chkPaths', 'chkOnlyEnts', 'chkBlockSize', 'txtBlockSize', 'chkSampleScale', 'txtSampleScale',
    'txtBSPOptions', 'chkLight', 'chkLightFast', 'chkLightExtra', 'chkLightVerbose', 'chkModelShadow',
    'chkNoModelShadow', 'chkDumpOptions', 'chkTraces', 'txtTraces', 'chkBounceFraction', 'txtBounceFraction',
    'chkJitter', 'txtJitter', 'txtLightOptions', 'chkRunMapAfterCompile', 'platformPc', 'gridCullModels'
  ];
  settings.controls = settings.controls || {};
  for (const id of ids) {
    const el = $(id);
    if (!el) continue;
    settings.controls[id] = el.type === 'checkbox' ? el.checked : el.value;
  }
  settings.mapSubFolder = mapSubFolder.value;
  settings.gridOption = document.querySelector('input[name="gridOption"]:checked')?.value || 'edit';
  saveSettings();
}

function restoreOptions() {
  if (settings.controls) {
    for (const [id, value] of Object.entries(settings.controls)) {
      const el = $(id);
      if (!el) continue;
      if (el.type === 'checkbox') el.checked = Boolean(value);
      else el.value = String(value);
    }
  }
  if (settings.mapSubFolder) mapSubFolder.value = settings.mapSubFolder;
  if (settings.gridOption) {
    const radio = document.querySelector(`input[name="gridOption"][value="${CSS.escape(settings.gridOption)}"]`);
    if (radio) radio.checked = true;
  }
  if (settings.mapstab === 'my') activeMapTab = 'my';
  tabAllMaps.classList.toggle('active', activeMapTab === 'all');
  tabMyMaps.classList.toggle('active', activeMapTab === 'my');
}

async function runSelectedMap() {
  cancelConsoleAutoClose();
  resetDownloads();
  setLogText('');
  if (inlineConsoleFallback) {
    inlineConsoleFallback.hidden = true;
    if ('open' in inlineConsoleFallback) inlineConsoleFallback.open = false;
  }
  openConsoleWindow(false);
  persistOptions();

  const choice = selectedMapChoice();
  if (!choice) {
    setStatus(runStatus, 'You must select a map to compile first.');
    return;
  }

  if (!currentSource || !currentSource.entries.length) {
    setStatus(runStatus, 'That path is not a valid CoD2 path.');
    return;
  }

  const compileBsp = Boolean($('chkBSP')?.checked);
  const compileVis = Boolean($('chkVis')?.checked);
  if (!compileBsp && !compileVis) {
    setStatus(runStatus, 'Select Compile BSP or Compile -Vis first.');
    return;
  }
  if (compileVis && choice.gameMode !== 'mp') {
    setStatus(runStatus, 'Compile -Vis is only enabled for detected MP maps.');
    return;
  }

  let canWriteOutputs = false;
  if (currentSource.type === 'directory' && currentSource.rootHandle) {
    try {
      canWriteOutputs = await requestDirectoryWritePermission(currentSource.rootHandle);
      if (!canWriteOutputs) {
        log('Write permission was not granted. Generated files will remain available as downloads.');
      }
    } catch (error) {
      log(`Write permission could not be requested: ${error.message || error}. Generated files will remain available as downloads.`);
    }
  }

  setStatus(runStatus, `Preparing ${choice.gameMode.toUpperCase()} map output ${displayPath(choice.compilePath)}...`);
  if (runButton) runButton.disabled = true;
  let files;
  try {
    files = await collectFilesForRun();
  } catch (error) {
    setStatus(runStatus, error.message || String(error));
    if (runButton) runButton.disabled = false;
    return;
  }

  latestRunId++;
  const runId = latestRunId;
  const worker = new Worker('runner.worker.js');
  const pendingOutputWrites = [];

  worker.addEventListener('message', event => {
    const message = event.data || {};
    if (message.runId !== runId) return;

    if (message.type === 'log') {
      log(message.line);
    } else if (message.type === 'status') {
      setStatus(runStatus, message.message);
      updateSummary();
    } else if (message.type === 'download') {
      const relativePath = message.relativePath || message.path.split('/').slice(-1)[0];
      const outputItem = addDownload(message.path, message.data, relativePath);
      setStatus(runStatus, `Generated ${message.path.split('/').pop()}.`);

      if (canWriteOutputs) {
        const writeTask = writeGeneratedOutputToDirectory(relativePath, message.data)
          .then(savedRel => {
            if (savedRel) {
              outputItem.meta.textContent = `${formatBytes(message.data.byteLength)} · saved to ${displayPath(savedRel)}`;
              setStatus(runStatus, `Saved ${displayPath(savedRel)}.`);
            } else {
              outputItem.meta.textContent = `${formatBytes(message.data.byteLength)} · download only`;
            }
          })
          .catch(error => {
            outputItem.meta.textContent = `${formatBytes(message.data.byteLength)} · download only`;
            log(`Could not write ${displayPath(relativePath)} to the selected folder: ${error.message || error}`);
          });
        pendingOutputWrites.push(writeTask);
      } else {
        outputItem.meta.textContent = `${formatBytes(message.data.byteLength)} · download only`;
      }
    } else if (message.type === 'done') {
      Promise.allSettled(pendingOutputWrites).then(() => {
        setStatus(runStatus, message.message);
        scheduleConsoleAutoClose(1500);
        if (runButton) runButton.disabled = false;
        worker.terminate();
      });
    } else if (message.type === 'error') {
      log(message.message);
      setStatus(runStatus, 'Compile failed. Open Console.');
      scheduleConsoleAutoClose(2500);
      if (runButton) runButton.disabled = false;
      worker.terminate();
    }
  });

  worker.addEventListener('error', event => {
    log(`${event.message} (${event.filename}:${event.lineno})`);
    setStatus(runStatus, 'Worker failed. Open Console.');
    scheduleConsoleAutoClose(2500);
    if (runButton) runButton.disabled = false;
    worker.terminate();
  });

  worker.postMessage({
    type: 'run',
    runId,
    selectedMap: choice.sourcePath,
    compileMap: choice.compilePath,
    loadFromPath: choice.loadFromPath,
    platformPc: $('platformPc').checked,
    compileBsp,
    compileVis,
    gameMode: choice.gameMode,
    bspArgs: bspOptionArgs(),
    visArgs: visOptionArgs(),
    mountRoot: currentSource.mountRoot || DEFAULT_MOUNT_ROOT,
    files
  });
}

async function chooseFolder() {
  if (window.showDirectoryPicker) {
    try {
      const handle = await window.showDirectoryPicker({ mode: 'read' });
      if (!(await requestDirectoryPermission(handle))) return;
      setStatus(sourceStatus, 'Indexing folder. File contents are not copied.');
      await saveDirectorySource(handle);
      await scanDirectorySource(handle);
    } catch (error) {
      if (error && error.name === 'AbortError') return;
      setStatus(sourceStatus, error.message || String(error));
    }
    return;
  }

  setStatus(sourceStatus, 'Directory handles are unavailable. Select a staged folder; files will be copied into OPFS.');
  filePicker.click();
}

async function stagePickedFiles() {
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
    savedSource = { type: 'opfs', name: 'OPFS staged files', savedAt: Date.now() };
    saveSavedSource();
    await scanOpfsSource();
  } catch (error) {
    setStatus(sourceStatus, error.message || String(error));
  } finally {
    filePicker.value = '';
  }
}

function toggleMyMap() {
  const choice = selectedMapChoice();
  if (!choice) return;
  const set = myMapsSet();
  if (set.has(choice.sourcePath)) set.delete(choice.sourcePath);
  else set.add(choice.sourcePath);
  settings.mymaps = [...set].sort();
  saveSettings();
  renderMapOptions();
}

function bindOptionPersistence() {
  document.querySelectorAll('input, select').forEach(el => {
    if (el.id === 'mapFilter' || el.id === 'txtTreePath') return;
    el.addEventListener('change', persistOptions);
  });
}

function markUnsupportedControls() {
  const unsupported = [
    'chkPaths', 'chkLight', 'chkLightFast', 'chkLightExtra', 'chkLightVerbose',
    'chkModelShadow', 'chkNoModelShadow', 'chkDumpOptions', 'chkTraces',
    'txtTraces', 'chkBounceFraction', 'txtBounceFraction', 'chkJitter',
    'txtJitter', 'txtLightOptions', 'chkRunMapAfterCompile', 'gridCullModels',
    'btnGrid', 'btnRunMap'
  ];
  for (const id of unsupported) {
    const el = $(id);
    if (!el) continue;
    el.disabled = true;
    el.title = 'Not available in the browser cod2map build.';
    const label = el.closest('label');
    if (label) label.classList.add('unsupported');
  }
}

$('chooseFolder').addEventListener('click', chooseFolder);
restoreButton.addEventListener('click', () => restoreSavedDirectory(false));
filePicker.addEventListener('change', stagePickedFiles);
$('refreshMaps').addEventListener('click', async () => {
  if (currentSource?.type === 'opfs') {
    await scanOpfsSource();
  } else if (currentSource?.entries) {
    setMapOptions(currentSource.entries.map(e => e.path));
  } else {
    setStatus(sourceStatus, 'No source is loaded.');
  }
});

tabAllMaps.addEventListener('click', () => setActiveTab('all'));
tabMyMaps.addEventListener('click', () => setActiveTab('my'));
myMapToggle.addEventListener('click', toggleMyMap);
mapFilter.addEventListener('input', renderMapOptions);
mapSubFolder.addEventListener('change', () => { persistOptions(); renderMapOptions(); });
mapSelect.addEventListener('change', () => {
  const choice = selectedMapChoice();
  if (choice) {
    settings.mapname = choice.sourcePath;
    saveSettings();
    setStatus(runStatus, `${choice.gameMode.toUpperCase()} map selected. Output: ${displayPath(choice.compilePath)}`);
  }
  updateMyMapButton();
});
$('openConsole')?.addEventListener('click', () => { openConsoleWindow(true); });
$('clearConsole')?.addEventListener('click', () => {
  setLogText('');
  if (inlineConsoleFallback) inlineConsoleFallback.hidden = true;
});
$('copyConsole')?.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText(logBuffer || '');
    setStatus(runStatus, 'Build log copied.');
  } catch (_) {
    setStatus(runStatus, 'Could not copy build log.');
  }
});
$('btnRunMap')?.addEventListener('click', () => {
  const choice = selectedMapChoice();
  setStatus(runStatus, choice ? `Selected ${displayPath(choice.sourcePath)}.` : 'You must select a map first.');
});
$('btnGrid')?.addEventListener('click', () => setStatus(runStatus, 'Grid mode is not implemented in the browser build.'));
runButton.addEventListener('click', runSelectedMap);

restoreOptions();
markUnsupportedControls();
bindOptionPersistence();
if (savedSource?.name) {
  txtTreePath.value = savedSource.name;
  restoreButton.disabled = false;
}
updateSummary(0, 0);
resetDownloads();
restoreSavedDirectory(true).catch(() => {});
