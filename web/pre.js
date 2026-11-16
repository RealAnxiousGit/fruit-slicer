// The browser version's save. SDL keeps the game's files under /libsdl, which lives only in memory, so
// that folder is backed by the browser's IndexedDB: loaded before the game starts, and written back a few
// seconds after anything changes, and again when the tab is hidden or closed.
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  FS.mkdir('/libsdl');
  FS.mount(IDBFS, {}, '/libsdl');
  addRunDependency('save');
  FS.syncfs(true, function (err) {
    if (err) console.warn('Fruit Slicer: the saved game could not be read', err);
    removeRunDependency('save');
  });

  var busy = false, again = false;
  function store() {
    if (busy) { again = true; return; }
    busy = true;
    FS.syncfs(false, function (err) {
      if (err) console.warn('Fruit Slicer: the game could not be saved', err);
      busy = false;
      if (again) { again = false; store(); }
    });
  }
  setInterval(store, 2000);
  document.addEventListener('visibilitychange', function () { if (document.hidden) store(); });
  window.addEventListener('pagehide', store);
});
