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
    },

    close() {},

    llseek(stream, offset, whence) {
      let position = offset;
      if (whence === 1) position += stream.position;
      else if (whence === 2) position += FS.isDir(stream.node.mode) ? 0 : getFileSize(stream.node);
      if (position < 0) throw errno(EINVAL);
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
      if (position >= size) return 0;
      const count = Math.min(length, size - position);

      if (contents.backing === 'blob') {
        if (typeof FileReaderSync !== 'function') {
          throw new Error('FileReaderSync is unavailable; this lazy VFS requires a Web Worker.');
        }
        const slice = contents.file.slice(position, position + count);
        const arrayBuffer = new FileReaderSync().readAsArrayBuffer(slice);
        buffer.set(new Uint8Array(arrayBuffer), offset);
        return count;
      }

      buffer.set(contents.data.subarray(position, position + count), offset);
      return count;
    },

    write(stream, buffer, offset, length, position) {
      const contents = stream.node.contents;
      if (!contents || contents.type !== 'file') throw errno(EINVAL);
      const end = position + length;
      ensureMemCapacity(contents, end);
      contents.data.set(buffer.subarray(offset, offset + length), position);
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

function countMountedIwds(FS, vroot) {
  let entries;
  try {
    entries = FS.readdir(`${vroot}/main`);
  } catch (e) {
    return { present: false, count: 0 };
  }
  return {
    present: true,
    count: entries.filter(name => /\.iwd$/i.test(name)).length
  };
}

function findGeneratedBsp(FS, selectedMap, vroot) {
  const base = `${vroot}/${normalizePath(selectedMap).replace(/\.map$/i, '')}`;
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

  walk(vroot);
  found.sort((a, b) => a.localeCompare(b));
  if (!found.length) return null;
  return { path: found[0], data: FS.readFile(found[0]) };
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

    const args = [];
    if (message.platformPc) args.push('-platform', 'pc');
    if (loadFromVPath) args.push('-loadFrom', loadFromVPath);
    args.push(compileVPath);

    post({ type: 'log', line: `Browser VFS mounted ${files.length} file reference(s) at ${vroot}. File contents are loaded only when cod2map reads them.` });
    const iwdInfo = countMountedIwds(module.FS, vroot);
    if (!iwdInfo.present) {
      post({ type: 'log', line: `Warning: ${vroot}/main is not visible in the browser VFS; CoD2 assets in main/*.iwd cannot be loaded.` });
    } else {
      post({ type: 'log', line: `Browser VFS sees ${iwdInfo.count} main/*.iwd file(s).` });
      if (!iwdInfo.count) {
        post({ type: 'log', line: 'Warning: no main/*.iwd files are mounted. Select the CoD2 install root containing main/iw_*.iwd.' });
      }
    }
    if (loadFromVPath) {
      post({ type: 'log', line: `Using source map ${loadFromVPath}; output path seed ${compileVPath}.` });
    }
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

    const bsp = findGeneratedBsp(module.FS, compileMap, vroot);
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
  }
}

self.addEventListener('message', event => {
  const message = event.data || {};
  if (message.type === 'run') {
    runCod2Map(message);
  }
});
