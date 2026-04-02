var Main = function () {

  // configurable
  var _interval = 66;
  var _colorBalance = false;

  // internal
  var _pause = 0;
  var _showStats = false;
  var _counter = 0;
  var _renderTime = 0;
  var _rafHandle = 0;
  var _lastFrameTs = 0;
  var _nextFrameAt = 0;
  var _selectedMode = 68;
  var _protocolVersion = 1;
  var _syncEvery = 32;
  var _profile = "R2";
  var _glContext = "none";
  var _glVersion = 0;
  var _modeLabels = {
    4: "4C",
    66: "Bu",
    67: "Bm",
    68: "B",
    69: "5x5",
    70: "5x5d",
  };
  var _modeValues = {
    4: 4,
    66: 66,
    67: 67,
    68: 68,
    69: 69,
    70: 70,
    "4C": 4,
    "4c": 4,
    "B": 68,
    "Bm": 67,
    "BM": 67,
    "Bu": 66,
    "BU": 66,
    "5x5": 69,
    "5X5": 69,
    "5x5d": 70,
    "5X5D": 70,
  };
  var _modeCssClasses = ["mode-b", "mode-bm", "mode-bu", "mode-4c", "mode-5x5", "mode-5x5d"];
  var _profiles = {
    R1: { mode: "Bm", fps: 10, colorBalance: false },
    R2: { mode: "B", fps: 15, colorBalance: false },
    R3: { mode: "Bu", fps: 20, colorBalance: false },
    X1: { mode: "5x5", fps: 18, colorBalance: false },
    X2: { mode: "5x5d", fps: 16, colorBalance: false },
  };

  var _wakeLock = undefined;

  // cached
  var _idealRatio = 1;
  var _compressBuff = undefined;

  function toggleFullscreen() {
    if (document.fullscreenElement) {
      return document.exitFullscreen();
    }
    else {
      return document.documentElement.requestFullscreen();
    }
  }

  function importFile(file) {
    let chunkSize = Module._cimbare_encode_bufsize();
    if (_compressBuff === undefined) {
      const dataPtr = Module._malloc(chunkSize);
      _compressBuff = new Uint8Array(Module.HEAPU8.buffer, dataPtr, chunkSize);
    }
    let offset = 0;
    let reader = new FileReader();

    Main.encode_init(file.name);

    reader.onload = function (event) {
      const datalen = event.target.result.byteLength;
      if (datalen > 0) {
        // copy to wasm buff and write
        const uint8View = new Uint8Array(event.target.result);
        _compressBuff.set(uint8View);
        const buffView = new Uint8Array(Module.HEAPU8.buffer, _compressBuff.byteOffset, datalen);
        Main.encode_bytes(buffView);

        offset += chunkSize;
        readNext();
      } else {
        // Done reading file
        console.log("Finished reading file.");

        // this null call is functionally a flush()
        // so a no-op, unless it isn't
        const nullBuff = new Uint8Array(Module.HEAPU8.buffer, _compressBuff.byteOffset, 0);
        Main.encode_bytes(nullBuff);
      }
    };

    function readNext() {
      let slice = file.slice(offset, offset + chunkSize);
      reader.readAsArrayBuffer(slice);
    }

    readNext();
  }

  function copyToWasmHeap(abuff) {
    const dataPtr = Module._malloc(abuff.length);
    const wasmData = new Uint8Array(Module.HEAPU8.buffer, dataPtr, abuff.length);
    wasmData.set(abuff);
    return wasmData;
  }

  function scheduleNextFrame() {
    if (_rafHandle) {
      return;
    }
    _rafHandle = requestAnimationFrame(Main.nextFrame);
  }

  function resolveModeValue(modeInput) {
    if (Object.prototype.hasOwnProperty.call(_modeValues, modeInput)) {
      return _modeValues[modeInput];
    }
    return 68;
  }

  function modeLabel(modeVal) {
    return _modeLabels[modeVal] || String(modeVal);
  }

  function supportsExtendedSenderRuntime() {
    return typeof Module._cimbare_get_mode === "function" &&
      typeof Module._cimbare_get_protocol_version === "function";
  }

  function sanitizeModeForRuntime(modeVal) {
    if (modeVal >= 69 && !supportsExtendedSenderRuntime()) {
      return 68;
    }
    return modeVal;
  }

  function updateCapabilityUI() {
    if (supportsExtendedSenderRuntime()) {
      return;
    }

    document.querySelectorAll(".mode-5x5, .mode-5x5d, .profile-x1, .profile-x2").forEach(function (node) {
      node.style.display = "none";
    });
  }

  function updateRendererUI(isReady, isCompatibilityMode) {
    var elem = document.getElementById("dragdrop");
    if (!elem) {
      return;
    }

    elem.classList.toggle("error", !isReady);
    elem.classList.toggle("compat", !!isCompatibilityMode);
  }

  // public interface
  return {
    init: function (canvas) {
      updateCapabilityUI();
      if (!Main.check_GL_enabled(canvas)) {
        Main.publishProtocolMetadata(0, 0);
        return;
      }
      Main.setProfile(_profile);
      Main.publishProtocolMetadata(0, 0);
    },

    check_GL_enabled: function (canvas) {
      var preferredVersion = Number(Module.cimbarPreferredWebGLVersion || 0);
      if (!preferredVersion) {
        _glContext = "none";
        _glVersion = 0;
        updateRendererUI(false, false);
        return false;
      }

      _glContext = preferredVersion >= 2 ? "webgl2" : "webgl";
      _glVersion = preferredVersion >= 2 ? 2 : 1;
      updateRendererUI(true, _glVersion < 2);
      return true;
    },

    resize: function () {
      // reset zoom
      var canvas = document.getElementById('canvas');
      var width = window.innerWidth - 10;
      var height = window.innerHeight - 10;
      Main.scaleCanvas(canvas, width, height);
      Main.alignInvisibleClick(canvas);
      Main.checkNavButtonOverlap();
    },

    toggleFullscreen: function () {
      toggleFullscreen().then(Main.resize);
      Main.togglePause(true);
    },

    togglePause: function (pause) {
      // pause is a cooldown. We pause to help autofocus, but we don't want to do it forever...
      if (pause === undefined) {
        pause = !Main.isPaused();
      }
      _pause = pause ? 15 : 0;
    },

    isPaused: function () {
      return _pause > 0;
    },

    scaleCanvas: function (canvas, width, height) {
      // using ratio from current config,
      // determine optimal dimensions and rotation
      var needRotate = _idealRatio > 1 && height > width;
      Module._cimbare_rotate_window(needRotate);

      var ourRatio = needRotate ? height / width : width / height;

      var xdim = needRotate ? height : width;
      var ydim = needRotate ? width : height;
      if (ourRatio > _idealRatio) {
        xdim = Math.floor(xdim * _idealRatio / ourRatio);
      }
      else if (ourRatio < _idealRatio) {
        ydim = Math.floor(ydim * ourRatio / _idealRatio);
      }

      console.log(xdim + "x" + ydim);
      if (needRotate) {
        canvas.style.width = ydim + "px";
        canvas.style.height = xdim + "px";
      }
      else {
        canvas.style.width = xdim + "px";
        canvas.style.height = ydim + "px";
      }
    },

    alignInvisibleClick: function (canvas) {
      canvas = canvas || document.getElementById('canvas');
      var cpos = canvas.getBoundingClientRect();
      var invisible_click = document.getElementById("invisible_click");
      invisible_click.style.width = canvas.style.width;
      invisible_click.style.height = canvas.style.height;
      invisible_click.style.top = cpos.top + "px";
      invisible_click.style.left = cpos.left + "px";
      invisible_click.style.zoom = canvas.style.zoom;
    },

    encode_init: function (filename) {
      console.log("encoding " + filename);
      const wasmFn = copyToWasmHeap(new TextEncoder("utf-8").encode(filename));
      try {
        var res = Module._cimbare_init_encode(wasmFn.byteOffset, wasmFn.length, -1);
        console.log("init_encode returned " + res);
      } finally {
        Module._free(wasmFn.byteOffset);
      }

      Main.setTitle(filename);
      Main.setHTML("current-file", filename);
    },

    prevent_sleep: async function () {
      if (_wakeLock) {
        return;
      }
      const requestWakeLock = async () => {
        try {
          _wakeLock = await navigator.wakeLock.request('screen');
          console.log('got wake lock!');
          _wakeLock.addEventListener('release', () => {
            _wakeLock = undefined;
          });
        } catch (err) { }
      };
      requestWakeLock();
    },

    encode_bytes: function (wasmData) {
      var res = Module._cimbare_encode(wasmData.byteOffset, wasmData.length);
      console.log("encode returned " + res);

      if (res == 0) {
        Main.setActive();
      }
    },

    dragDrop: function (event) {
      console.log("drag drop?");
      console.log(event);
      const files = event.dataTransfer.files;
      if (files && files.length === 1) {
        importFile(files[0]);
      }
    },

    checkNavButtonOverlap: function () {
      var nav = document.getElementById("nav-button");
      var navBounds = nav.getBoundingClientRect();
      var canvas = document.getElementById('canvas').getBoundingClientRect();
      if (navBounds.right > canvas.left && navBounds.bottom > canvas.top) {
        nav.classList.add("hide");
      }
      else {
        nav.classList.remove("hide");
      }
    },

    clickNav: function () {
      document.getElementById("nav-button").focus();
    },

    blurNav: function (pause) {
      if (pause === undefined) {
        pause = true;
      }
      document.getElementById("nav-button").blur();
      document.getElementById("nav-content").blur();
      document.getElementById("nav-find-file-link").blur();
      Main.togglePause(pause);
    },

    clickFileInput: function () {
      document.getElementById("file_input").click();
    },

    fileInput: function (ev) {
      console.log("file input: " + ev);
      var file = document.getElementById('file_input').files[0];
      if (file) {
        importFile(file);
      }
      Main.blurNav(false);
    },

    nextFrame: function (timestamp) {
      _rafHandle = 0;
      timestamp = timestamp || performance.now();
      if (_nextFrameAt && timestamp < _nextFrameAt) {
        scheduleNextFrame();
        return;
      }

      _counter += 1;
      if (_pause > 0) {
        _pause -= 1;
      }
      var start = performance.now();
      var frameCount = 0;
      if (!Main.isPaused()) {
        Module._cimbare_render();
        frameCount = Module._cimbare_next_frame(_colorBalance);
      }

      var elapsed = performance.now() - start;
      var frameInterval = _lastFrameTs ? (timestamp - _lastFrameTs) : 0;
      _lastFrameTs = timestamp;
      _nextFrameAt = timestamp + _interval;
      scheduleNextFrame();
      Main.publishProtocolMetadata(frameCount, frameInterval);

      if (_showStats && frameCount) {
        _renderTime += elapsed;
        Main.setHTML("status", elapsed + " : " + frameCount + " : " + Math.ceil(_renderTime / frameCount) + " : " + frameInterval.toFixed(2));
      }

      if (!Main.isPaused() && _counter % 16 == 0) {
        setTimeout(Main.prevent_sleep, 0);
      }
    },

    setActive: function (active) {
      // hide cursor when there's a barcode active
      var invisi = document.getElementById("invisible_click");
      invisi.classList.remove("active");
      invisi.classList.add("active");
    },

    updateNavMode: function (modeVal) {
      var nav = document.getElementById("nav-container");
      if (!nav) {
        return;
      }
      _modeCssClasses.forEach(function (modeClass) {
        nav.classList.remove(modeClass);
      });

      const cssClass = {
        4: "mode-4c",
        66: "mode-bu",
        67: "mode-bm",
        68: "mode-b",
        69: "mode-5x5",
        70: "mode-5x5d",
      }[modeVal];
      if (cssClass) {
        nav.classList.add(cssClass);
      }
    },

    setMode: function (modeInput, fromProfile) {
      const modeVal = sanitizeModeForRuntime(resolveModeValue(modeInput));
      Module._cimbare_configure(modeVal, -1);
      _selectedMode = typeof Module._cimbare_get_mode === "function" ? Module._cimbare_get_mode() : modeVal;
      _protocolVersion = typeof Module._cimbare_get_protocol_version === "function" ? Module._cimbare_get_protocol_version() : 0;
      if (!fromProfile) {
        _profile = "manual";
      }
      _idealRatio = Module._cimbare_get_aspect_ratio();
      Main.resize();
      Main.updateNavMode(_selectedMode);
      Main.publishProtocolMetadata(0, 0);
    },

    setProfile: function (profileName) {
      const profile = _profiles[profileName];
      if (!profile) {
        return;
      }
      _profile = profileName;
      _colorBalance = profile.colorBalance;
      Main.setFPS(profile.fps);
      Main.setMode(profile.mode, true);
      Main.publishProtocolMetadata(0, 0);
    },

    publishProtocolMetadata: function (frameCount, frameInterval) {
      const selectedModeName = modeLabel(_selectedMode);
      const metadata = {
        protocolVersion: _protocolVersion,
        profile: _profile,
        mode: _selectedMode,
        modeName: selectedModeName,
        frameCounter: _counter,
        frameCount: frameCount || 0,
        frameIntervalMs: frameInterval || 0,
        syncEvery: _syncEvery,
        isReferenceFrame: _counter > 0 && (_counter % _syncEvery === 0),
        experimentalMode: supportsExtendedSenderRuntime() && _selectedMode >= 69,
        glContext: _glContext,
        glVersion: _glVersion,
      };

      window.CIMBAR_PROTOCOL_METADATA = metadata;
      var nav = document.getElementById("nav-container");
      if (nav) {
        nav.dataset.protocolVersion = String(metadata.protocolVersion);
        nav.dataset.profile = String(metadata.profile);
        nav.dataset.mode = String(metadata.mode);
        nav.dataset.modeName = String(metadata.modeName);
        nav.dataset.frameCounter = String(metadata.frameCounter);
        nav.dataset.frameCount = String(metadata.frameCount);
        nav.dataset.frameIntervalMs = metadata.frameIntervalMs.toFixed(2);
        nav.dataset.syncEvery = String(metadata.syncEvery);
        nav.dataset.referenceFrame = metadata.isReferenceFrame ? "1" : "0";
        nav.dataset.experimentalMode = metadata.experimentalMode ? "1" : "0";
        nav.dataset.glContext = String(metadata.glContext);
        nav.dataset.glVersion = String(metadata.glVersion);
      }
    },

    setFPS: function (val) {
      if (!val) {
        return;
      }
      _interval = Math.floor(1000 / val);
      console.log("new frame delay interval is " + _interval);
    },

    setHTML: function (id, msg) {
      document.getElementById(id).innerHTML = msg;
    },

    setTitle: function (msg) {
      document.title = "Cimbar: " + msg;
    }
  };
}();

window.addEventListener('keydown', function (e) {
  e = e || event;
  if (e.target instanceof HTMLBodyElement) {
    if (e.key == 'Enter' || e.keyCode == 13 ||
      e.key == 'Tab' || e.keyCode == 9 ||
      e.key == 'Space' || e.keyCode == 32
    ) {
      Main.clickNav();
      e.preventDefault();
    }
    else if (e.key == 'Backspace' || e.keyCode == 8) {
      Main.togglePause(true);
      e.preventDefault();
    }
  }
  else {
    if (e.key == 'Escape' || e.keyCode == 27 ||
      e.key == 'Backspace' || e.keyCode == 8 ||
      e.key == 'End' || e.keyCode == 35 ||
      e.key == 'Home' || e.keyCode == 36
    ) {
      Main.blurNav();
    }
    else if (e.key == 'Tab' || e.keyCode == 9 ||
      e.key == 'ArrowDown' || e.keyCode == 40
    ) {
      var nav = document.getElementById('nav-button');
      var links = document.getElementById('nav-content').getElementsByTagName('a');
      if (nav.classList.contains('attention')) {
        nav.classList.remove('attention');
        links[0].classList.add('attention');
        return;
      }
      for (var i = 0; i < links.length; i++) {
        if (links[i].classList.contains('attention')) {
          var next = i + 1 == links.length ? nav : links[i + 1];
          links[i].classList.remove('attention');
          next.classList.add('attention');
          break;
        }
      }
    }
    else if (e.key == 'ArrowUp' || e.keyCode == 38) {
      var nav = document.getElementById('nav-button');
      var links = document.getElementById('nav-content').getElementsByTagName('a');
      if (nav.classList.contains('attention')) {
        nav.classList.remove('attention');
        links[links.length - 1].classList.add('attention');
        return;
      }

      for (var i = 0; i < links.length; i++) {
        if (links[i].classList.contains('attention')) {
          var next = i == 0 ? nav : links[i - 1];
          links[i].classList.remove('attention');
          next.classList.add('attention');
          break;
        }
      }
    }
    else if (e.key == 'Enter' || e.keyCode == 13 ||
      e.key == ' ' || e.keyCode == 32
    ) {
      var nav = document.getElementById('nav-button');
      if (nav.classList.contains('attention')) {
        Main.blurNav();
        return;
      }
      var links = document.getElementById('nav-content').getElementsByTagName('a');
      for (var i = 0; i < links.length; i++) {
        if (links[i].classList.contains('attention')) {
          links[i].click();
        }
      }
    }
  }
}, true);

window.addEventListener("touchstart", function (e) {
  e = e || event;
  Main.togglePause(true);
}, false);

window.addEventListener("touchend", function (e) {
  e = e || event;
  Main.togglePause(false);
}, false);

window.addEventListener("touchcancel", function (e) {
  e = e || event;
  Main.togglePause(false);
}, false);

window.addEventListener("dragover", function (e) {
  e = e || event;
  e.preventDefault();

  document.body.style["opacity"] = 0.5;
}, false);

window.addEventListener("dragleave", function (e) {
  e = e || event;
  e.preventDefault();

  document.body.style["opacity"] = 1.0;
}, false);

window.addEventListener("drop", function (e) {
  e = e || event;
  e.preventDefault();
  e.stopPropagation();
  Main.dragDrop(e);
  document.body.style["opacity"] = 1.0;
}, false);

window.addEventListener('resize', () => {
  Main.resize();
});
