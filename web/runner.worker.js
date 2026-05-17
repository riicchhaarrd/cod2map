'use strict';

const VROOT = '/game';
let currentRun = 0;

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

function basename(path) {
  const i = path.lastIndexOf('/');
  return i === -1 ? path : path.slice(i + 1);
}

function mkdirp(FS, path) {
  const clean = path.replace(/\/+/g, '/').replace(/\/$/, '');
  if (!clean || clean === '/') return;
  const parts = clean.split('/').filter(Boolean);
  let cur = '';
  for (const part of parts) {
    cur += `/${part}`;
    try {
      FS.mkdir(cur);
    } catch (e) {
      // Existing directories are expected when many files share a parent.
    }
  }
}

function mountLazyFile(FS, entry, objectUrls) {
  const rel = normalizePath(entry.path);
  if (!rel || rel.includes('..')) {
    throw new Error(`Rejected unsafe virtual path: ${entry.path}`);
  }

  const vpath = `${VROOT}/${rel}`;
  const parent = dirname(vpath);
  const name = basename(vpath);
  mkdirp(FS, parent);

  try {
    FS.unlink(vpath);
  } catch (e) {
    // File did not already exist.
  }

  const url = URL.createObjectURL(entry.file);
  objectUrls.push(url);

  if (typeof FS.createLazyFile !== 'function') {
    throw new Error('This Emscripten build does not expose FS.createLazyFile. Rebuild with FORCE_FILESYSTEM enabled.');
  }

  FS.createLazyFile(parent || '/', name, url, true, false);
}

function findGeneratedBsp(FS, selectedMap) {
  const base = `${VROOT}/${normalizePath(selectedMap).replace(/\.map$/i, '')}`;
  const candidates = [
    `${base}.d3dbsp`,
    `${base}.d3dbsp_xenon`,
    `${base}.bsp`
  ];

  for (const c of candidates) {
    try {
      const data = FS.readFile(c);
      return { path: c, data };
    } catch (e) {
      // Not generated under this exact name.
    }
  }

  const found = [];
  function walk(path) {
    let list;
    try { list = FS.readdir(path); } catch (e) { return; }
    for (const name of list) {
      if (name === '.' || name === '..') continue;
      const p = `${path}/${name}`;
      let st;
      try { st = FS.stat(p); } catch (e) { continue; }
      if (FS.isDir(st.mode)) walk(p);
      else if (/\.d3d.*bsp$/i.test(name) || /\.bsp$/i.test(name)) found.push(p);
    }
  }

  walk(VROOT);
  found.sort((a, b) => a.localeCompare(b));
  if (!found.length) return null;
  return { path: found[0], data: FS.readFile(found[0]) };
}

async function runCod2Map(message) {
  const runId = message.runId || ++currentRun;
  const objectUrls = [];

  const post = payload => {
    self.postMessage({ runId, ...payload });
  };

  try {
    post({ type: 'status', message: 'Loading WebAssembly module...' });
    importScripts('cod2map.js');

    const module = await createCod2Map({
      noInitialRun: true,
      print: text => post({ type: 'log', line: String(text) }),
      printErr: text => post({ type: 'log', line: String(text) })
    });

    mkdirp(module.FS, VROOT);

    const files = message.files || [];
    post({ type: 'status', message: `Mapping ${files.length} file(s) into lazy VFS...` });

    for (let i = 0; i < files.length; i++) {
      mountLazyFile(module.FS, files[i], objectUrls);
      if ((i + 1) % 500 === 0) {
        post({ type: 'status', message: `Mapped ${i + 1}/${files.length} file(s) into lazy VFS...` });
      }
    }

    const args = [];
    if (message.platformPc) args.push('-platform', 'pc');
    args.push(`${VROOT}/${normalizePath(message.selectedMap)}`);

    post({ type: 'log', line: `Lazy VFS mapped ${files.length} file(s). File contents are loaded only when cod2map opens them.` });
    post({ type: 'status', message: `Running cod2map ${args.join(' ')}` });

    let exitCode = 0;
    try {
      exitCode = module.callMain(args);
    } catch (e) {
      if (typeof e?.status === 'number') exitCode = e.status;
      else throw e;
    }

    if (exitCode !== 0) {
      post({ type: 'done', exitCode, message: `cod2map exited with code ${exitCode}.` });
      return;
    }

    const bsp = findGeneratedBsp(module.FS, message.selectedMap);
    if (!bsp) {
      post({ type: 'done', exitCode: 0, message: 'cod2map finished, but no generated BSP was found. Check console output.' });
      return;
    }

    const output = new Uint8Array(bsp.data);
    self.postMessage({
      runId,
      type: 'download',
      path: bsp.path,
      data: output.buffer
    }, [output.buffer]);

    post({ type: 'done', exitCode: 0, message: `Generated ${bsp.path}.` });
  } catch (error) {
    post({
      type: 'error',
      message: error && error.stack ? error.stack : String(error)
    });
  } finally {
    for (const url of objectUrls) URL.revokeObjectURL(url);
  }
}

self.addEventListener('message', event => {
  const message = event.data || {};
  if (message.type === 'run') {
    runCod2Map(message);
  }
});
