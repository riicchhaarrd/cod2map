'use strict';

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

function normalizeVirtualRoot(path) {
  const clean = `/${normalizePath(path || '/vfs/game')}`.replace(/\/+/g, '/').replace(/\/$/, '');
  if (!clean.startsWith('/vfs/') || clean.includes('..')) return '/vfs/game';
  return clean;
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

function createBrowserBackedFilesystem(module) {
  const FS = module.FS;
  const now = () => Date.now();
  const DIR_MODE = 0o40777;
  const FILE_MODE = 0o100666;
  const BLOB_READ_CACHE_SIZE = 256 * 1024;
  const ENOENT = 44;
  const EINVAL = 28;
  const EISDIR = 31;
  const ENOTDIR = 54;

  function errno(code) {
    return new FS.ErrnoError(code);
  }

  function makeEmptyTree() {
    return { type: 'dir', children: Object.create(null) };
  }

  function addTreeFile(root, relPath, file) {
    const parts = relPath.split('/').filter(Boolean);
    if (!parts.length) return;
    let cur = root;
    for (let i = 0; i < parts.length - 1; i++) {
      const part = parts[i];
      let next = cur.children[part];
      if (!next) {
        next = makeEmptyTree();
        cur.children[part] = next;
      }
      if (next.type !== 'dir') throw new Error(`Path component is not a directory: ${parts.slice(0, i + 1).join('/')}`);
      cur = next;
    }
    cur.children[parts[parts.length - 1]] = {
      type: 'file',
      backing: 'blob',
      file,
      size: file.size,
      timestamp: now()
    };
  }

  function allocMemFile(size = 0) {
    return {
      type: 'file',
      backing: 'mem',
      data: new Uint8Array(Math.max(0, size)),
      size: Math.max(0, size),
      timestamp: now()
    };
  }

  function getFileSize(node) {
    const contents = node.contents;
    if (!contents || contents.type !== 'file') return 0;
    if (contents.backing === 'blob') return contents.file.size;
    return contents.size;
  }

  function resizeMemFile(contents, size) {
    const nextSize = Math.max(0, size | 0);
    if (contents.backing === 'blob') {
      contents.backing = 'mem';
      contents.data = new Uint8Array(nextSize);
    } else if (contents.data.length !== nextSize) {
      const next = new Uint8Array(nextSize);
      next.set(contents.data.subarray(0, Math.min(contents.size, nextSize)));
      contents.data = next;
    }
    contents.size = nextSize;
    contents.timestamp = now();
  }

  function ensureMemCapacity(contents, minSize) {
    if (contents.backing === 'blob') {
      contents.backing = 'mem';
      contents.data = new Uint8Array(Math.max(minSize, contents.file.size));
      contents.size = contents.file.size;
      return;
    }
    if (contents.data.length >= minSize) return;
    let capacity = contents.data.length || 1;
    while (capacity < minSize) capacity *= 2;
    const next = new Uint8Array(capacity);
    next.set(contents.data.subarray(0, contents.size));
    contents.data = next;
  }

  const nodeOps = {
    getattr(node) {
      const isDir = FS.isDir(node.mode);
      const size = isDir ? 4096 : getFileSize(node);
      const timestamp = node.contents?.timestamp || now();
      return {
        dev: 1,
        ino: node.id,
        mode: node.mode,
        nlink: isDir ? 2 : 1,
        uid: 0,
        gid: 0,
        rdev: 0,
        size,
        atime: new Date(timestamp),
        mtime: new Date(timestamp),
        ctime: new Date(timestamp),
        blksize: 4096,
        blocks: Math.ceil(size / 4096)
      };
    },

    setattr(node, attr) {
      if (attr.mode !== undefined) node.mode = attr.mode;
      if (attr.timestamp !== undefined && node.contents) node.contents.timestamp = attr.timestamp;
      if (attr.size !== undefined && node.contents?.type === 'file') resizeMemFile(node.contents, attr.size);
    },

    lookup(parent, name) {
      const child = parent.contents?.children?.[name];
      if (!child) throw errno(ENOENT);
      return child;
    },

    mknod(parent, name, mode, dev) {
      if (!parent.contents || parent.contents.type !== 'dir') throw errno(EINVAL);
      const node = FS.createNode(parent, name, mode, dev);
      node.node_ops = nodeOps;
      node.stream_ops = streamOps;
      node.contents = FS.isDir(mode)
        ? { type: 'dir', children: Object.create(null), timestamp: now() }
        : allocMemFile(0);
      parent.contents.children[name] = node;
      parent.contents.timestamp = now();
      return node;
    },

    rename(oldNode, newDir, newName) {
      if (!oldNode.parent?.contents?.children || !newDir.contents?.children) throw errno(EINVAL);
      delete oldNode.parent.contents.children[oldNode.name];
      oldNode.name = newName;
      oldNode.parent = newDir;
      newDir.contents.children[newName] = oldNode;
      newDir.contents.timestamp = now();
    },

    unlink(parent, name) {
      if (!parent.contents?.children?.[name]) throw errno(ENOENT);
      delete parent.contents.children[name];
      parent.contents.timestamp = now();
    },

    rmdir(parent, name) {
      const child = parent.contents?.children?.[name];
      if (!child) throw errno(ENOENT);
      if (child.contents?.children && Object.keys(child.contents.children).length) throw errno(EINVAL);
      delete parent.contents.children[name];
      parent.contents.timestamp = now();
    },

    readdir(node) {
      if (!node.contents || node.contents.type !== 'dir') throw errno(EINVAL);
      return ['.', '..', ...Object.keys(node.contents.children)];
    }
  };

  const streamOps = {
    open(stream) {
      // Directories are opened by libc opendir/readdir; regular file reads and writes
      // are handled below. Do not reject directory opens here.
      if (stream.node?.contents?.backing === 'blob') {
        stream.browserBlobCache = { start: -1, end: -1, data: null };
      }
    },

    close(stream) {
      if (stream) stream.browserBlobCache = null;
    },

    llseek(stream, offset, whence) {
      let position = Number(offset || 0);
      if (whence === 1) position += Number(stream.position || 0);
      else if (whence === 2) position += FS.isDir(stream.node.mode) ? 0 : getFileSize(stream.node);
      if (!Number.isFinite(position) || position < 0) throw errno(EINVAL);
      return position;
    },

    readdir(stream) {
      if (!FS.isDir(stream.node.mode)) throw errno(ENOTDIR);
      const entries = nodeOps.readdir(stream.node);
      if (stream.position >= entries.length) return null;
      return entries[stream.position++];
    },

    read(stream, buffer, offset, length, position) {
      const contents = stream.node.contents;
      if (!contents || contents.type !== 'file') throw errno(EINVAL);
      const size = getFileSize(stream.node);
      const pos = position === undefined || position === null ? Number(stream.position || 0) : Number(position);
      if (!Number.isFinite(pos) || pos < 0) throw errno(EINVAL);
      if (pos >= size) return 0;
      const count = Math.min(length, size - pos);

      if (contents.backing === 'blob') {
        if (typeof FileReaderSync !== 'function') {
          throw new Error('FileReaderSync is unavailable; this lazy VFS requires a Web Worker.');
        }

        const cache = stream.browserBlobCache || (stream.browserBlobCache = { start: -1, end: -1, data: null });
        if (count <= BLOB_READ_CACHE_SIZE && cache.data && pos >= cache.start && pos + count <= cache.end) {
          buffer.set(cache.data.subarray(pos - cache.start, pos - cache.start + count), offset);
          return count;
        }

        if (count <= BLOB_READ_CACHE_SIZE) {
          const readEnd = Math.min(size, pos + BLOB_READ_CACHE_SIZE);
          const arrayBuffer = new FileReaderSync().readAsArrayBuffer(contents.file.slice(pos, readEnd));
          cache.start = pos;
          cache.end = readEnd;
          cache.data = new Uint8Array(arrayBuffer);
          buffer.set(cache.data.subarray(0, count), offset);
          return count;
        }

        const slice = contents.file.slice(pos, pos + count);
        const arrayBuffer = new FileReaderSync().readAsArrayBuffer(slice);
        buffer.set(new Uint8Array(arrayBuffer), offset);
        return count;
      }

      buffer.set(contents.data.subarray(pos, pos + count), offset);
      return count;
    },

    write(stream, buffer, offset, length, position) {
      const contents = stream.node.contents;
      if (!contents || contents.type !== 'file') throw errno(EINVAL);
      const pos = position === undefined || position === null ? Number(stream.position || 0) : Number(position);
      if (!Number.isFinite(pos) || pos < 0) throw errno(EINVAL);
      const end = pos + length;
      ensureMemCapacity(contents, end);
      contents.data.set(buffer.subarray(offset, offset + length), pos);
      contents.size = Math.max(contents.size, end);
      contents.timestamp = now();
      return length;
    }
  };

  function createNode(parent, name, treeEntry, mode) {
    const node = FS.createNode(parent, name, mode, 0);
    node.node_ops = nodeOps;
    node.stream_ops = streamOps;

    if (treeEntry.type === 'dir') {
      node.contents = { type: 'dir', children: Object.create(null), timestamp: now() };
      for (const childName of Object.keys(treeEntry.children).sort()) {
        const childEntry = treeEntry.children[childName];
        const childMode = childEntry.type === 'dir' ? DIR_MODE : FILE_MODE;
        const childNode = createNode(node, childName, childEntry, childMode);
        node.contents.children[childName] = childNode;
      }
    } else {
      node.contents = treeEntry;
    }

    return node;
  }

  return {
    buildTree(entries) {
      const root = makeEmptyTree();
      for (const entry of entries) {
        const rel = normalizePath(entry.path);
        if (!rel || rel.includes('..')) throw new Error(`Rejected unsafe virtual path: ${entry.path}`);
        addTreeFile(root, rel, entry.file);

        // CoD2 map sources commonly reference prefabs as "prefabs/...", while
        // editor/source packages may store them under "map_source/prefabs/...".
        // Keep the original path and add a read-only compatibility alias so
        // stock maps can resolve prefab references without copying file bytes.
        const lower = rel.toLowerCase();
        if (lower.startsWith('map_source/prefabs/')) {
          addTreeFile(root, rel.slice('map_source/'.length), entry.file);
        } else if (lower.startsWith('prefabs/')) {
          addTreeFile(root, `map_source/${rel}`, entry.file);
        }

        // Some mod-tool dumps keep collision-map sources under map_source/collmaps
        // while the compiler searches collmaps/, and some hand-curated folders
        // do the reverse. Add both lightweight aliases when either layout exists.
        if (lower.startsWith('map_source/collmaps/')) {
          addTreeFile(root, rel.slice('map_source/'.length), entry.file);
        } else if (lower.startsWith('collmaps/')) {
          addTreeFile(root, `map_source/${rel}`, entry.file);
        }
      }
      return root;
    },

    mount(mount) {
      return createNode(null, '/', mount.opts.tree, DIR_MODE);
    }
  };
}

function mountBrowserVfs(module, entries, vroot) {
  const FS = module.FS;
  const lazyFs = createBrowserBackedFilesystem(module);
  const tree = lazyFs.buildTree(entries);

  mkdirp(FS, dirname(vroot));
  try {
    FS.mkdir(vroot);
  } catch (e) {
    // Existing mount point is expected on repeated runs in a fresh module setup.
  }

  FS.mount(lazyFs, { tree }, vroot);
}

function probeMountedIwds(FS, vroot) {
  let entries;
  try {
    entries = FS.readdir(`${vroot}/main`);
  } catch (e) {
    return { present: false, count: 0, names: [], probes: [] };
  }

  const names = entries
    .filter(name => name !== '.' && name !== '..' && /\.iwd$/i.test(name))
    .sort((a, b) => a.localeCompare(b));

  const probes = [];
  for (const name of names.slice(0, 3)) {
    const path = `${vroot}/main/${name}`;
    const probe = { name, path, readable: false, size: 0, magic: '' };
    try {
      const st = FS.stat(path);
      probe.size = st.size || 0;
      const fd = FS.open(path, 'r');
      try {
        const buf = new Uint8Array(4);
        const n = FS.read(fd, buf, 0, 4, 0);
        probe.readable = n === 4;
        probe.magic = Array.from(buf.subarray(0, n))
          .map(b => b.toString(16).padStart(2, '0'))
          .join(' ');
      } finally {
        FS.close(fd);
      }
    } catch (e) {
      probe.error = e && e.message ? e.message : String(e);
    }
    probes.push(probe);
  }

  return { present: true, count: names.length, names, probes };
}

function relativeFromVroot(path, vroot) {
  const cleanPath = `/${normalizePath(path)}`.replace(/\/+/g, '/');
  const cleanRoot = normalizeVirtualRoot(vroot);
  if (cleanPath === cleanRoot) return '';
  if (cleanPath.startsWith(`${cleanRoot}/`)) return normalizePath(cleanPath.slice(cleanRoot.length + 1));
  return basename(cleanPath);
}

function collectGeneratedOutputs(FS, selectedMap, vroot) {
  const base = `${vroot}/${normalizePath(selectedMap).replace(/\.map$/i, '')}`;
  const outputExts = ['.d3dbsp', '.d3dbsp_xenon', '.bsp', '.lin', '.prt', '.reg'];
  const found = new Map();

  function addIfReadable(path) {
    if (found.has(path)) return;
    try {
      const data = FS.readFile(path);
      found.set(path, { path, relativePath: relativeFromVroot(path, vroot), data });
    } catch (e) {
      // Not generated or not readable.
    }
  }

  for (const ext of outputExts) addIfReadable(`${base}${ext}`);

  if (!found.size) {
    const expectedDir = dirname(base);
    function walk(path) {
      let list;
      try { list = FS.readdir(path); } catch (e) { return; }
      for (const name of list) {
        if (name === '.' || name === '..') continue;
        const p = `${path}/${name}`;
        let st;
        try { st = FS.stat(p); } catch (e) { continue; }
        if (FS.isDir(st.mode)) walk(p);
        else if (/\.(d3dbsp|d3dbsp_xenon|bsp|lin|prt|reg)$/i.test(name)) addIfReadable(p);
      }
    }
    walk(expectedDir);
  }

  return [...found.values()].sort((a, b) => {
    const rank = ext => ['.d3dbsp', '.bsp', '.d3dbsp_xenon', '.lin', '.prt', '.reg'].indexOf(ext.toLowerCase());
    const aExt = a.relativePath.match(/\.[^.]+$/)?.[0] || '';
    const bExt = b.relativePath.match(/\.[^.]+$/)?.[0] || '';
    const byRank = rank(aExt) - rank(bExt);
    return byRank || a.relativePath.localeCompare(b.relativePath);
  });
}

async function runCod2Map(message) {
  const runId = message.runId || ++currentRun;

  const post = payload => {
    self.postMessage({ runId, ...payload });
  };

  try {
    post({ type: 'status', message: 'Loading WebAssembly module...' });
    importScripts('cod2map.js');

    const module = await createCod2Map({
      noInitialRun: true,
      noExitRuntime: true,
      print: text => post({ type: 'log', line: String(text) }),
      printErr: text => post({ type: 'log', line: String(text) })
    });

    const files = message.files || [];
    const vroot = normalizeVirtualRoot(message.mountRoot);
    post({ type: 'status', message: `Mounting ${files.length} file reference(s) into browser VFS...` });
    mountBrowserVfs(module, files, vroot);
    mkdirp(module.FS, `${vroot}/bin`);
    module.FS.chdir(`${vroot}/bin`);

    const selectedMap = normalizePath(message.selectedMap);
    const compileMap = normalizePath(message.compileMap || selectedMap);
    const loadFromPath = normalizePath(message.loadFromPath || '');
    const compileVPath = `${vroot}/${compileMap}`;
    const loadFromVPath = loadFromPath ? `${vroot}/${loadFromPath}` : '';
    mkdirp(module.FS, dirname(compileVPath));

    const compileBsp = message.compileBsp !== false;
    const compileVis = Boolean(message.compileVis);

    const cleanArgs = value => Array.isArray(value)
      ? value.map(arg => String(arg || '').trim()).filter(Boolean)
      : [];

    const buildPhaseArgs = phase => {
      const args = [];
      if (phase === 'vis') args.push('-vis');
      if (message.platformPc) args.push('-platform', 'pc');
      args.push(...cleanArgs(phase === 'vis' ? message.visArgs : message.bspArgs));
      if (phase === 'bsp' && loadFromVPath) args.push('-loadFrom', loadFromVPath);
      args.push(compileVPath);
      return args;
    };

    const runPhase = phase => {
      const args = buildPhaseArgs(phase);
      post({ type: 'status', message: `Running cod2map ${args.join(' ')}` });
      post({ type: 'log', line: `Running cod2map ${args.join(' ')}` });
      try {
        return module.callMain(args);
      } catch (e) {
        if (typeof e?.status === 'number') return e.status;
        throw e;
      }
    };

    post({ type: 'log', line: `Browser VFS mounted ${files.length} file reference(s) at ${vroot}. File contents are loaded only when cod2map reads them.` });
    post({ type: 'log', line: 'Browser VFS aliases prefabs and collmaps between root and map_source when those folders are present.' });
    const iwdInfo = probeMountedIwds(module.FS, vroot);
    if (!iwdInfo.present) {
      post({ type: 'log', line: `Warning: ${vroot}/main is not visible in the browser VFS; CoD2 assets in main/*.iwd cannot be loaded.` });
    } else {
      const sample = iwdInfo.names.slice(0, 6).join(', ') || 'none';
      post({ type: 'log', line: `Browser VFS sees ${iwdInfo.count} main/*.iwd file(s). Sample: ${sample}.` });
      for (const probe of iwdInfo.probes) {
        if (probe.readable) {
          post({ type: 'log', line: `IWD probe: ${probe.name}, ${probe.size} byte(s), first bytes ${probe.magic}.` });
        } else {
          post({ type: 'log', line: `Warning: IWD probe failed for ${probe.name}: ${probe.error || 'not readable'}.` });
        }
      }
      if (!iwdInfo.count) {
        post({ type: 'log', line: 'Warning: no main/*.iwd files are mounted. Select the CoD2 install root containing main/iw_*.iwd.' });
      }
    }
    if (loadFromVPath) {
      post({ type: 'log', line: `Using source map ${loadFromVPath}; output path seed ${compileVPath}.` });
    }
    post({ type: 'log', line: `Detected ${String(message.gameMode || 'unknown').toUpperCase()} map; output seed ${compileVPath}.` });

    let exitCode = 0;
    if (compileBsp) {
      exitCode = runPhase('bsp');
      if (exitCode !== 0) {
        post({ type: 'done', exitCode, message: `cod2map BSP exited with code ${exitCode}.` });
        return;
      }
    }

    if (compileVis) {
      exitCode = runPhase('vis');
      if (exitCode !== 0) {
        post({ type: 'done', exitCode, message: `cod2map Vis exited with code ${exitCode}.` });
        return;
      }
    }

    const outputs = collectGeneratedOutputs(module.FS, compileMap, vroot);
    if (!outputs.length) {
      post({ type: 'done', exitCode: 0, message: 'cod2map finished, but no generated outputs were found. Check console output.' });
      return;
    }

    for (const generated of outputs) {
      const output = new Uint8Array(generated.data);
      self.postMessage({
        runId,
        type: 'download',
        path: generated.path,
        relativePath: generated.relativePath,
        data: output.buffer
      }, [output.buffer]);
    }

    const names = outputs.map(o => o.relativePath || basename(o.path)).join(', ');
    post({ type: 'done', exitCode: 0, message: `Generated ${names}.` });
  } catch (error) {
    post({
      type: 'error',
      message: error && error.stack ? error.stack : String(error)
    });
  }
}

self.addEventListener('message', event => {
  const message = event.data || {};
  if (message.type === 'run') {
    runCod2Map(message);
  }
});
