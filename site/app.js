// Progressive enhancements for the 3dsFM website. The page works without
// this file. Everything here plays
// along with the DS and 3DS theme: the HOME Menu features, the status bar
// clock, PictoChat and Notifications behaviour, and the menu sounds. Nothing
// is hidden until the script has confirmed it is running, and motion is
// skipped when reduced motion is requested.
(function () {
  "use strict";

  var root = document.documentElement;
  root.classList.add("js");

  var REPO = "ankushKun/3dsFM";
  var PAGE_URL = location.origin + location.pathname;
  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  var SPRING = "cubic-bezier(0.34, 1.56, 0.64, 1)";

  // Any part of the page can ask for a menu sound by name; setUpSounds fills
  // this in, and until then (or with sounds off) it does nothing.
  var sfx = { play: function () {} };

  function formatBytes(bytes) {
    if (bytes < 1024) return Math.round(bytes) + " B";
    var units = ["KiB", "MiB", "GiB"];
    var value = bytes / 1024;
    var unit = 0;
    while (value >= 1024 && unit < units.length - 1) {
      value /= 1024;
      unit += 1;
    }
    return value.toFixed(value < 10 ? 2 : 1) + " " + units[unit];
  }

  // Latest release: the version on the hero sticker, and the file sizes.
  function showRelease() {
    fetch("https://api.github.com/repos/" + REPO + "/releases/latest", {
      headers: { Accept: "application/vnd.github+json" }
    })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.json();
      })
      .then(function (release) {
        document.querySelectorAll("[data-release-tag]").forEach(function (element) {
          element.textContent = "New! " + release.tag_name;
          element.hidden = false;
        });
        (release.assets || []).forEach(function (asset) {
          document
            .querySelectorAll('[data-asset-size="' + asset.name + '"]')
            .forEach(function (element) {
              element.textContent = formatBytes(asset.size);
            });
        });
      })
      .catch(function () {
        // Leave the static text in place when GitHub can't be reached.
      });
  }

  function copyText(text, button) {
    var label = button.querySelector("span");
    var original = label ? label.textContent : "";
    var done = function (message) {
      if (!label) return;
      label.textContent = message;
      setTimeout(function () {
        label.textContent = original;
      }, 1600);
    };
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(
        function () { done("Copied"); sfx.play("copy"); },
        function () { done("Copy failed"); }
      );
    } else {
      done("Copy failed");
    }
  }

  function setUpCopyButtons() {
    document.querySelectorAll("[data-copy]").forEach(function (button) {
      button.addEventListener("click", function () {
        copyText(button.getAttribute("data-copy"), button);
      });
    });
  }

  // The song on the 3DS's screens plays on a loop: the progress line and
  // the times tick, and at the end the next song takes over, like the app.
  function setUpDeviceDemo() {
    var bars = document.querySelectorAll("[data-demo-bar]");
    var positions = document.querySelectorAll("[data-demo-pos]");
    var titles = document.querySelectorAll("[data-demo-title]");
    var artists = document.querySelectorAll("[data-demo-artist]");
    var albums = document.querySelectorAll("[data-demo-album]");
    var lengths = document.querySelectorAll("[data-demo-len]");
    var nextArtist = document.querySelector("[data-demo-next-artist][data-demo-next='1']");
    var state = document.querySelector("[data-demo-state]");
    if (!bars.length || !titles.length || reduceMotion) return;

    var songs = [
      { id: "tDgjTP8fuz4", title: "Not Tomorrow (Long Version)", artist: "Akira Yamaoka", length: 176 },
      { id: "ZwLvcaDMhU8", title: "Promise (Reprise)", artist: "Akira Yamaoka", length: 105 },
      { id: "lXoVugTertI", title: "Subwoofer Lullaby", artist: "C418", length: 209 },
      { id: "5w3rRFWzjcM", title: "Sweden", artist: "C418", length: 216 },
      { id: "enHmnTPsp9w", title: "I Want Love (Studio Mix)", artist: "Akira Yamaoka", length: 281 },
      { id: "jb1SkDjaXk8", title: "Obstacles", artist: "Syd Matters", length: 210 },
      { id: "Qg6BwvDcANg", title: "Tessellate", artist: "alt-J", length: 188 },
      { id: "maWHojZTnTg", title: "Aria Math", artist: "C418", length: 310 },
      { id: "_ZaeKhITDWw", title: "Hazardous Environments", artist: "Valve", length: 83 },
      { id: "td3P1-cfZ4E", title: "Theme of Laura", artist: "Akira Yamaoka", length: 205 },
      { id: "2zPGWGqdfJ8", title: "oh yeah?", artist: "Steve Lacy", length: 171 },
      { id: "MX7FSAnRPug", title: "Loser", artist: "Tame Impala", length: 224 },
      { id: "SCzjQePVhl8", title: "To All of You", artist: "Syd Matters", length: 284 },
      { id: "UkumV_dHbkg", title: "You're Not Here", artist: "Akira Yamaoka", length: 226 },
      { id: "D1Fw6CbGAX4", title: "Running Up That Hill (A Deal With God)", artist: "Kate Bush", length: 299 },
      { id: "Kf5pXDhx5Vc", title: "End of Beginning", artist: "Djo", length: 160 },
      { id: "cuMuMnCRfqk", title: "Dracula", artist: "Tame Impala", length: 206 },
      { id: "-SaUrDhp034", title: "Every Breath You Take", artist: "The Police", length: 254 },
      { id: "Y1-Qymcppps", title: "Everybody Wants To Rule The World", artist: "Tears for Fears", length: 252 },
      { id: "HzdD8kbDzZA", title: "Take on Me", artist: "a-ha", length: 226 },
      { id: "7qlU0bWMzUU", title: "Never Ending Story", artist: "Gaten Matarazzo & Gabriella Pizzolo", length: 126 },
      { id: "wjZMcWaniA4", title: "Should I Stay or Should I Go (Remastered)", artist: "The Clash", length: 189 },
      { id: "fEnpB9fPUbg", title: "Want You Gone", artist: "Aperture Science Psychoacoustic Laboratories", length: 144 },
      { id: "i5_asj1BGFs", title: "Sweet Dreams (Are Made of This)", artist: "Eurythmics", length: 218 },
      { id: "qGBohd0V2Mo", title: "Video Killed The Radio Star", artist: "The Buggles", length: 254 },
      { id: "lYBUbBu4W08", title: "Never Gonna Give You Up", artist: "Rick Astley", length: 214 },
      { id: "Ha2OcL_0gtM", title: "Kids", artist: "Kyle Dixon & Michael Stein", length: 158 },
      { id: "qnleYf6Qmfk", title: "Vs Red HeartGold SoulSilver (Pokemon EPIC REMIX)", artist: "Cyastorber", length: 186 }
    ];
    // A different order on every visit, like shuffle on the console.
    for (var i = songs.length - 1; i > 0; i -= 1) {
      var j = Math.floor(Math.random() * (i + 1));
      var swap = songs[i];
      songs[i] = songs[j];
      songs[j] = swap;
    }
    var covers = document.querySelectorAll("[data-demo-cover]");
    var nextCovers = document.querySelectorAll("[data-demo-next-cover]");
    var nextRows = {
      title: document.querySelectorAll("[data-demo-next-title]"),
      artist: document.querySelectorAll("[data-demo-next-artist]"),
      length: document.querySelectorAll("[data-demo-next-len]")
    };
    var index = 0;
    var position = Math.floor(songs[0].length * 0.37);
    var pauseUntil = 0;

    function clock(seconds) {
      return Math.floor(seconds / 60) + ":" + ("0" + (seconds % 60)).slice(-2);
    }
    function setAll(list, text) {
      list.forEach(function (element) { element.textContent = text; });
    }
    function cover(song) {
      return "assets/covers/" + song.id + ".jpg";
    }
    function show() {
      var song = songs[index];
      setAll(titles, song.title);
      setAll(artists, song.artist);
      setAll(albums, "Top Picks · Playlist");
      setAll(lengths, clock(song.length));
      setAll(positions, clock(position));
      bars.forEach(function (bar) { bar.style.setProperty("--p", (position / song.length).toFixed(4)); });
      covers.forEach(function (image) { image.src = cover(song); });
      // The next three songs fill the up-next rows; the first also sits in
      // the top screen's corner.
      for (var n = 1; n <= 3; n += 1) {
        var upcoming = songs[(index + n) % songs.length];
        var at = function (list) {
          return Array.prototype.filter.call(list, function (element) {
            return (element.getAttribute("data-demo-next") || "1") === String(n);
          });
        };
        setAll(at(nextRows.title), upcoming.title);
        setAll(at(nextRows.artist), n === 1 && nextArtist ? "Up next · " + upcoming.artist : upcoming.artist);
        setAll(at(nextRows.length), clock(upcoming.length));
        at(nextCovers).forEach(function (image) { image.src = cover(upcoming); });
      }
      if (state) state.textContent = "Now playing · MP3 · " + (songs.length - 1) + " songs queued";
    }
    show();

    // Time passes eight times faster than life, so a visit sees a few songs.
    setInterval(function () {
      if (document.hidden) return;
      var now = Date.now();
      if (now < pauseUntil) return;
      position += 1;
      if (position >= songs[index].length) {
        index = (index + 1) % songs.length;
        position = 0;
        pauseUntil = now + 600;
      }
      show();
    }, 125);
  }

  // The screens are drawn at native 3DS size and projected onto the photo by
  // fixed matrices computed for the photo at --photo-w pixels wide, so they
  // need scaling by (rendered width / --photo-w). The factor is set here and
  // kept current as the stage resizes.
  function setUpScreenScale() {
    // Feature demos: a 3DS-pixel canvas scaled to its box.
    document.querySelectorAll(".shot").forEach(function (shot) {
      var base = shot.classList.contains("shot-bot") ? 320 : 400;
      function fit() {
        if (shot.clientWidth > 0) shot.style.setProperty("--ks", (shot.clientWidth / base).toFixed(5));
      }
      fit();
      if ("ResizeObserver" in window) new ResizeObserver(fit).observe(shot); else window.addEventListener("resize", fit);
    });
    document.querySelectorAll(".device-stage").forEach(function (stage) {
      var photoWidth = parseFloat(getComputedStyle(stage).getPropertyValue("--photo-w"));
      if (!photoWidth) return;
      function update() {
        var width = stage.offsetWidth;
        if (width > 0) stage.style.setProperty("--k", (width / photoWidth).toFixed(5));
      }
      update();
      if ("ResizeObserver" in window) {
        new ResizeObserver(update).observe(stage);
      } else {
        window.addEventListener("resize", update);
      }
      stage.classList.add("is-mapped");
    });
  }

  // A slight tilt of the device towards the pointer. Pointer only (no touch),
  // and off when reduced motion is requested.
  function setUpTilt() {
    var device = document.querySelector("[data-tilt]");
    var stage = device && device.querySelector(".device-stage");
    if (!device || !stage || reduceMotion) return;
    if (!window.matchMedia("(hover: hover) and (pointer: fine)").matches) return;

    var frame = null;
    function apply(x, y) {
      stage.style.transform = "rotateX(" + (y * -4).toFixed(2) + "deg) rotateY(" + (x * 6).toFixed(2) + "deg)";
    }
    device.addEventListener("pointermove", function (event) {
      var box = device.getBoundingClientRect();
      var x = (event.clientX - box.left) / box.width - 0.5;
      var y = (event.clientY - box.top) / box.height - 0.5;
      if (frame) cancelAnimationFrame(frame);
      frame = requestAnimationFrame(function () { apply(x, y); });
    });
    device.addEventListener("pointerleave", function () {
      if (frame) cancelAnimationFrame(frame);
      stage.style.transform = "";
    });
  }

  // The FAQ is the Notifications app: each question is marked new until it
  // is opened, and the browser remembers which ones were read.
  function setUpNotifications() {
    var faq = document.querySelector(".faq");
    var count = document.querySelector("[data-unread]");
    if (!faq) return;
    var rows = Array.prototype.slice.call(faq.querySelectorAll("details[id]"));
    var KEY = "3dsfm-read";
    var read = [];
    try {
      read = JSON.parse(localStorage.getItem(KEY) || "[]");
      if (!Array.isArray(read)) read = [];
    } catch (error) {
      read = [];
    }

    function update() {
      var unread = rows.filter(function (row) { return !row.classList.contains("is-read"); }).length;
      if (!count) return;
      count.textContent = unread ? unread + " new" : "All read";
      count.classList.toggle("is-done", !unread);
    }

    rows.forEach(function (row) {
      if (read.indexOf(row.id) !== -1) row.classList.add("is-read");
      row.addEventListener("toggle", function () {
        if (!row.open || row.classList.contains("is-read")) return;
        row.classList.add("is-read");
        read.push(row.id);
        try {
          localStorage.setItem(KEY, JSON.stringify(read));
        } catch (error) {
          // Remembering is a nicety; the row still reads as read this visit.
        }
        update();
      });
      var summary = row.querySelector("summary");
      summary.addEventListener("click", function () {
        var opening = !row.open || row.classList.contains("is-closing");
        sfx.play(opening ? "open" : "close");
      });
    });
    update();
    // Rows already read appear read at once; only later changes animate.
    requestAnimationFrame(function () {
      requestAnimationFrame(function () { faq.classList.add("is-ready"); });
    });
  }

  // FAQ rows open and close with an eased height animation and a short fade
  // of the answer. Without the script, or with reduced motion, <details>
  // simply opens and closes on its own.
  function setUpFaq() {
    if (reduceMotion || !("animate" in Element.prototype)) return;
    var timing = { duration: 320, easing: "cubic-bezier(0.23, 1, 0.32, 1)" };

    document.querySelectorAll(".faq details").forEach(function (details) {
      var summary = details.querySelector("summary");
      var answer = summary && summary.nextElementSibling;
      if (!answer) return;
      var running = null;

      function settle(open) {
        details.open = open;
        details.classList.remove("is-closing");
        details.style.overflow = "";
        running = null;
      }

      function animate(from, to, opening) {
        if (running) running.cancel();
        details.style.overflow = "hidden";
        running = details.animate({ height: [from + "px", to + "px"] }, timing);
        answer.animate(
          opening
            ? { opacity: [0, 1], transform: ["translateY(-6px)", "none"] }
            : { opacity: [1, 0], transform: ["none", "translateY(-4px)"] },
          { duration: opening ? 280 : 200, easing: timing.easing, fill: "none" }
        );
        var current = running;
        // finished rejects when a later click cancels this animation; the
        // newer animation then owns the cleanup.
        running.finished.then(function () {
          if (running === current) settle(opening);
        }, function () {});
      }

      summary.addEventListener("click", function (event) {
        event.preventDefault();
        // Start from the height on screen, even mid-animation. Fractional
        // sizes avoid a 1px snap when the animation hands over.
        var height = function (element) { return element.getBoundingClientRect().height; };
        var style = getComputedStyle(details);
        var frame = parseFloat(style.borderTopWidth) + parseFloat(style.borderBottomWidth);
        var from = height(details);
        if (details.open && !details.classList.contains("is-closing")) {
          details.classList.add("is-closing");
          animate(from, height(summary) + frame, false);
        } else {
          details.classList.remove("is-closing");
          details.open = true;
          animate(from, height(summary) + height(answer) + frame, true);
        }
      });
    });
  }

  // The install tabs. Arrow keys, Home and End move between them (a roving
  // tabindex, as in the ARIA tabs pattern). Switching slides the old QR code
  // and details off both screens and the new ones on, in the tab's direction.
  function setUpInstaller() {
    var picker = document.querySelector(".picker");
    var panel = document.getElementById("install-screens");
    if (!picker || !panel) return;
    var picks = Array.prototype.slice.call(picker.querySelectorAll(".pick"));
    var animated = !reduceMotion && "animate" in Element.prototype;
    var easeOut = "cubic-bezier(0.23, 1, 0.32, 1)";
    var easeIn = "cubic-bezier(0.4, 0, 1, 1)";
    var views = Array.prototype.slice.call(panel.querySelectorAll(".view"));
    var turn = 0;

    function stopAll() {
      views.forEach(function (view) {
        view.getAnimations({ subtree: true }).forEach(function (animation) { animation.cancel(); });
      });
    }

    // On phones the touch screen is too small to read, so its views move out
    // below the console (and back when there is room again).
    var stepsBox = panel.querySelector(".install-steps");
    var bottomScreen = panel.querySelector(".screen-bottom");
    var infoViews = views.filter(function (view) { return view.classList.contains("info-view"); });
    var narrow = window.matchMedia("(max-width: 640px)");
    function placeSteps() {
      if (!stepsBox || !bottomScreen) return;
      var split = narrow.matches;
      panel.classList.toggle("is-split", split);
      infoViews.forEach(function (view) {
        (split ? stepsBox : bottomScreen).appendChild(view);
      });
    }
    placeSteps();
    if (narrow.addEventListener) {
      narrow.addEventListener("change", placeSteps);
    } else if (narrow.addListener) {
      narrow.addListener(placeSteps);
    }

    function showOnly(method) {
      views.forEach(function (view) { view.hidden = view.dataset.view !== method; });
    }

    function swap(method, direction) {
      var mine = ++turn;
      stopAll();
      var leaving = views.filter(function (view) {
        return !view.hidden && view.dataset.view !== method;
      });
      if (!animated || !leaving.length) {
        showOnly(method);
        return;
      }

      var exits = leaving.map(function (view, index) {
        return view.animate(
          { opacity: [1, 0], transform: ["none", "translateX(" + -direction * 24 + "px)"] },
          { duration: 160, delay: index * 30, easing: easeIn, fill: "forwards" }
        ).finished;
      });
      // A rejection means a newer click cancelled this swap and owns the views.
      Promise.all(exits).then(function () {
        if (mine !== turn) return;
        showOnly(method);
        stopAll();
        views.filter(function (view) { return !view.hidden; }).forEach(function (view, index) {
          var timing = { duration: 320, delay: index * 50, easing: easeOut, fill: "backwards" };
          view.animate(
            { opacity: [0, 1], transform: ["translateX(" + direction * 24 + "px)", "none"] },
            timing
          );
          var tile = view.querySelector(".qr-tile");
          if (tile) {
            tile.animate({ transform: ["scale(0.92)", "none"] }, { duration: 380, easing: easeOut, fill: "backwards" });
          }
        });
      }, function () {});
    }

    function select(pick, focus) {
      if (focus) pick.focus();
      if (pick.getAttribute("aria-selected") === "true") return;
      var before = picks.findIndex(function (other) {
        return other.getAttribute("aria-selected") === "true";
      });
      picks.forEach(function (other) {
        var on = other === pick;
        other.setAttribute("aria-selected", on ? "true" : "false");
        other.tabIndex = on ? 0 : -1;
      });
      picker.dataset.active = pick.dataset.method;
      panel.setAttribute("aria-labelledby", pick.id);
      sfx.play("swipe");
      swap(pick.dataset.method, picks.indexOf(pick) > before ? 1 : -1);
    }

    picks.forEach(function (pick) {
      pick.addEventListener("click", function () { select(pick, false); });
    });

    picker.addEventListener("keydown", function (event) {
      var index = picks.indexOf(document.activeElement);
      if (index < 0) return;
      var next = {
        ArrowLeft: (index - 1 + picks.length) % picks.length,
        ArrowRight: (index + 1) % picks.length,
        Home: 0,
        End: picks.length - 1
      }[event.key];
      if (next === undefined) return;
      event.preventDefault();
      select(picks[next], true);
    });
  }

  // The status bar clock, in the visitor's own time and format, with the
  // colon blinking like the 3DS's.
  function setUpClock() {
    var clock = document.querySelector("[data-clock]");
    if (!clock || !window.Intl) return;
    var hours = clock.querySelector("[data-hours]");
    var minutes = clock.querySelector("[data-minutes]");
    var format = new Intl.DateTimeFormat(undefined, { hour: "2-digit", minute: "2-digit" });
    function tick() {
      var parts = {};
      format.formatToParts(new Date()).forEach(function (part) { parts[part.type] = part.value; });
      hours.textContent = parts.hour || "--";
      minutes.textContent = (parts.minute || "--") + (parts.dayPeriod ? " " + parts.dayPeriod : "");
      setTimeout(tick, 60000 - (Date.now() % 60000) + 50);
    }
    tick();
  }

  // The features as a HOME Menu: a tab per icon (arrow keys move between
  // them, as in the ARIA tabs pattern), a selection frame that glides to the
  // selected icon, and the selected feature on the "top screen". While the
  // section is in view and nobody has touched it, it browses on its own.
  function setUpHome() {
    var home = document.querySelector("[data-home]");
    if (!home) return;
    var grid = home.querySelector(".home-grid");
    var cursor = home.querySelector(".home-cursor");
    var icons = Array.prototype.slice.call(home.querySelectorAll(".app-icon"));
    var panels = icons.map(function (icon) {
      return document.getElementById(icon.getAttribute("aria-controls"));
    });
    if (!grid || !cursor || !icons.length) return;
    var index = Math.max(0, icons.findIndex(function (icon) {
      return icon.getAttribute("aria-selected") === "true";
    }));

    // Layout positions (offsets ignore the selected icon's lift and scale).
    function place() {
      var icon = icons[index];
      var glyph = icon.querySelector(".app-glyph");
      if (!glyph || !icon.offsetWidth) return;
      var margin = 6;
      cursor.style.width = glyph.offsetWidth + margin * 2 + "px";
      cursor.style.height = glyph.offsetHeight + margin * 2 + "px";
      cursor.style.transform = "translate(" +
        (icon.offsetLeft + glyph.offsetLeft - margin) + "px, " +
        (icon.offsetTop + glyph.offsetTop - margin) + "px)";
    }

    function select(next, how) {
      if (next === index) {
        if (how === "key") icons[next].focus();
        return;
      }
      index = next;
      icons.forEach(function (icon, i) {
        var on = i === next;
        icon.setAttribute("aria-selected", on ? "true" : "false");
        icon.tabIndex = on ? 0 : -1;
        if (panels[i]) panels[i].classList.toggle("is-on", on);
      });
      place();
      if (how === "key") icons[next].focus();
      if (how !== "auto") sfx.play("select");
    }

    place();
    requestAnimationFrame(function () { cursor.classList.add("is-ready"); });
    if ("ResizeObserver" in window) new ResizeObserver(place).observe(grid);
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(place);

    icons.forEach(function (icon, i) {
      icon.addEventListener("click", function () { select(i, "click"); });
    });

    grid.addEventListener("keydown", function (event) {
      var columns = getComputedStyle(grid).gridTemplateColumns.split(" ").length || icons.length;
      var last = icons.length - 1;
      var next = {
        ArrowLeft: index === 0 ? last : index - 1,
        ArrowRight: index === last ? 0 : index + 1,
        ArrowUp: Math.max(0, index - columns),
        ArrowDown: Math.min(last, index + columns),
        Home: 0,
        End: last
      }[event.key];
      if (next === undefined) return;
      event.preventDefault();
      select(next, "key");
    });

    // Attract mode: off with reduced motion, paused while the pointer is over
    // the menu or the section is out of view, and over for good once someone
    // picks an icon themselves.
    if (reduceMotion || !("IntersectionObserver" in window)) return;
    var timer = null;
    var visible = false;
    var hovering = false;
    var done = false;
    function stop() {
      clearInterval(timer);
      timer = null;
    }
    function run() {
      stop();
      if (done || !visible || hovering) return;
      timer = setInterval(function () {
        if (!document.hidden) select((index + 1) % icons.length, "auto");
      }, 4200);
    }
    new IntersectionObserver(function (entries) {
      visible = entries[0].isIntersecting;
      run();
    }, { threshold: 0.5 }).observe(home);
    home.addEventListener("pointerenter", function () { hovering = true; run(); });
    home.addEventListener("pointerleave", function () { hovering = false; run(); });
    ["pointerdown", "keydown", "focusin"].forEach(function (type) {
      home.addEventListener(type, function () { done = true; stop(); });
    });
  }

  // Scroll reveals. The reveal-ready class is added only here, so sections
  // are never hidden unless this code is running and can show them again.
  function setUpReveals() {
    var items = document.querySelectorAll(".reveal");
    if (!items.length || reduceMotion || !("IntersectionObserver" in window)) return;
    root.classList.add("reveal-ready");
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          entry.target.classList.add("is-in");
          observer.unobserve(entry.target);
        }
      });
    }, { rootMargin: "0px 0px -8% 0px", threshold: 0.08 });
    items.forEach(function (item) { observer.observe(item); });
  }

  // Highlight the tab for the section in view.
  function setUpTabs() {
    var tabs = Array.prototype.slice.call(document.querySelectorAll(".tabs a"));
    if (!tabs.length || !("IntersectionObserver" in window)) return;
    var byId = {};
    tabs.forEach(function (tab) { byId[tab.getAttribute("href").slice(1)] = tab; });
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        var tab = byId[entry.target.id];
        if (!tab) return;
        if (entry.isIntersecting) {
          tabs.forEach(function (other) { other.classList.toggle("is-active", other === tab); });
        }
      });
    }, { rootMargin: "-40% 0px -50% 0px" });
    Object.keys(byId).forEach(function (id) {
      var section = document.getElementById(id);
      if (section) observer.observe(section);
    });
  }

  // Menu sounds, synthesised on the fly (no recordings of anyone's console).
  // On by default; browsers only allow audio after the first click or key
  // press, so they start from then.
  function setUpSounds() {
    var toggle = document.getElementById("sound-toggle");
    var AudioContextClass = window.AudioContext || window.webkitAudioContext;
    if (!toggle || !AudioContextClass) {
      if (toggle) toggle.hidden = true;
      return;
    }
    var context = null;
    var enabled = true;
    var lastHover = 0;

    try {
      enabled = localStorage.getItem("3dsfm-sounds") !== "off";
    } catch (error) {
      enabled = true;
    }

    function unlock() {
      if (!context) context = new AudioContextClass();
      if (context.state === "suspended") context.resume();
    }
    window.addEventListener("pointerdown", unlock, { passive: true });
    window.addEventListener("keydown", unlock);

    // One short note: a pitch glide with a quick attack and an exponential
    // fade, optionally delayed so notes can make a little tune.
    function tone(from, to, duration, volume, type, delay) {
      var start = context.currentTime + (delay || 0);
      var oscillator = context.createOscillator();
      var gain = context.createGain();
      oscillator.type = type || "sine";
      oscillator.frequency.setValueAtTime(from, start);
      oscillator.frequency.exponentialRampToValueAtTime(to, start + duration);
      gain.gain.setValueAtTime(0.0001, start);
      gain.gain.linearRampToValueAtTime(volume, start + 0.006);
      gain.gain.exponentialRampToValueAtTime(0.0001, start + duration);
      oscillator.connect(gain).connect(context.destination);
      oscillator.start(start);
      oscillator.stop(start + duration + 0.02);
    }

    var sounds = {
      hover: function () { tone(1800, 1500, 0.03, 0.03); },
      click: function () { tone(520, 880, 0.09, 0.06); },
      select: function () {
        tone(1175, 1175, 0.05, 0.045, "triangle");
        tone(1568, 1568, 0.07, 0.04, "triangle", 0.05);
      },
      swipe: function () { tone(420, 980, 0.12, 0.05); },
      open: function () { tone(660, 1100, 0.1, 0.055); },
      close: function () { tone(900, 520, 0.1, 0.05); },
      copy: function () {
        tone(784, 784, 0.08, 0.045, "triangle");
        tone(988, 988, 0.08, 0.045, "triangle", 0.06);
        tone(1319, 1319, 0.14, 0.045, "triangle", 0.12);
      },
      message: function () {
        tone(1319, 1319, 0.07, 0.05, "triangle");
        tone(1760, 1760, 0.12, 0.05, "triangle", 0.08);
      },
      toggle: function () { tone(660, 990, 0.08, 0.06); }
    };

    sfx.play = function (name) {
      if (!enabled || !context || context.state !== "running") return;
      (sounds[name] || sounds.click)();
    };

    function render() {
      toggle.setAttribute("aria-pressed", enabled ? "true" : "false");
      toggle.querySelector("use").setAttribute("href", enabled ? "#i-sound" : "#i-mute");
      toggle.querySelector("span").textContent = enabled ? "Menu sounds on" : "Menu sounds off";
    }
    render();

    toggle.addEventListener("click", function () {
      enabled = !enabled;
      try {
        localStorage.setItem("3dsfm-sounds", enabled ? "on" : "off");
      } catch (error) {
        // Remembering the choice is a nicety; it still applies to this visit.
      }
      render();
      sfx.play("toggle");
    });

    document.addEventListener("pointerover", function (event) {
      var target = event.target.closest("[data-sound], .btn, .tabs a, .app-icon, .pick, .faq summary, .sbtn");
      if (!target || target.contains(event.relatedTarget)) return;
      var now = Date.now();
      if (now - lastHover < 70) return;
      lastHover = now;
      sfx.play("hover");
    });
    // Buttons and tabs click; the menu, installer and FAQ play their own.
    document.addEventListener("click", function (event) {
      var target = event.target.closest("[data-sound], .btn, .tabs a, .sbtn, .brand");
      if (target) sfx.play(target.getAttribute("data-sound") || "click");
    });
  }

  showRelease();
  setUpCopyButtons();
  setUpScreenScale();
  setUpDeviceDemo();
  setUpTilt();
  setUpClock();
  setUpHome();
  setUpNotifications();
  setUpFaq();
  setUpInstaller();
  setUpReveals();
  setUpTabs();
  setUpSounds();
})();

// The buddy card on the right edge: the sibling app, peeking in. Hover
// opens it on a pointer; a tap on its tab opens and closes it on touch.
// It gives one handwritten "psst!" after the page has settled and wiggles
// its icon every so often.
(function () {
  "use strict";
  var buddy = document.getElementById("buddy");
  if (!buddy) return;
  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  var tab = buddy.querySelector(".buddy-tab");
  tab.addEventListener("click", function (event) {
    event.preventDefault();
    buddy.classList.toggle("is-open");
    buddy.classList.remove("is-psst");
  });
  buddy.addEventListener("pointerenter", function () { buddy.classList.remove("is-psst"); });
  if (location.search.indexOf("buddy=open") !== -1) buddy.classList.add("is-open");

  if (reduceMotion) return;
  // A nudge and a "psst!" once the reader has had a moment, then a wiggle
  // every so often while the card is tucked away.
  setTimeout(function () {
    if (buddy.matches(":hover") || buddy.classList.contains("is-open")) return;
    buddy.classList.add("is-nudge", "is-psst");
    setTimeout(function () { buddy.classList.remove("is-nudge"); }, 900);
    setTimeout(function () { buddy.classList.remove("is-psst"); }, 4200);
  }, 2600);
  setInterval(function () {
    if (buddy.matches(":hover") || buddy.classList.contains("is-open")) return;
    buddy.classList.add("is-wiggle");
    setTimeout(function () { buddy.classList.remove("is-wiggle"); }, 800);
  }, 11000);
})();
