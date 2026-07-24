# Alpine/postmarketOS Install Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Install Xfce Dark Mode Switcher on Alpine/postmarketOS and document the required Alpine-specific paths and packages.

**Architecture:** Keep the plugin build unchanged. Use make-time variables `PREFIX=/usr LIBDIR=/usr/lib` for Alpine/postmarketOS installation because Xfce panel plugins are discovered in `/usr/lib/xfce4/panel/plugins` and metadata in `/usr/share/xfce4/panel/plugins`.

**Tech Stack:** C, GTK 3, Xfce panel plugin API, GNU make-compatible make, Alpine `apk`, Xfce panel runtime tools.

## Global Constraints

- Target system is postmarketOS edge with `ID_LIKE=alpine`.
- System install uses `PREFIX=/usr LIBDIR=/usr/lib`.
- Required pkg-config modules are `gtk+-3.0` and `libxfce4panel-2.0`.
- Use passwordless `sudo` for `apk add` and `make install`.
- Update docs and changelog for Alpine/postmarketOS.

---

### Task 1: Local Alpine/postmarketOS installation

**Files:**
- Build from: `src/darkmode-plugin.c`
- Generate: `libdarkmode-switcher.so`
- Install: `/usr/lib/xfce4/panel/plugins/libdarkmode-switcher.so`
- Install: `/usr/share/xfce4/panel/plugins/darkmode-switcher.desktop`

**Interfaces:**
- Consumes: `Makefile` targets `clean`, `all`, and `install`.
- Produces: a system-discoverable Xfce panel plugin named `darkmode-switcher`.

- [ ] **Step 1: Install dependencies**

Run:

```sh
sudo apk add build-base pkgconf gtk+3.0-dev xfce4-panel-dev xfconf gsettings-desktop-schemas
```

- [ ] **Step 2: Verify pkg-config dependencies**

Run:

```sh
pkg-config --modversion gtk+-3.0 libxfce4panel-2.0
```

Expected: both modules print versions.

- [ ] **Step 3: Build with Alpine paths**

Run:

```sh
make clean
make all PREFIX=/usr LIBDIR=/usr/lib
```

Expected: `libdarkmode-switcher.so` and `data/darkmode-switcher.desktop` are created.

- [ ] **Step 4: Install system-wide**

Run:

```sh
sudo make install PREFIX=/usr LIBDIR=/usr/lib
```

Expected: installed files exist under `/usr/lib/xfce4/panel/plugins` and `/usr/share/xfce4/panel/plugins`.

- [ ] **Step 5: Restart and add to panel**

Run:

```sh
xfce4-panel -r
xfce4-panel --add=darkmode-switcher
```

Expected: the panel restarts and attempts to add the plugin.

### Task 2: Alpine/postmarketOS documentation update

**Files:**
- Modify: `README.md`
- Modify: `docs/INSTALL.md`
- Modify: `docs/TROUBLESHOOTING.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: verified install command sequence from Task 1.
- Produces: documentation that Alpine/postmarketOS users can copy without Debian multiarch paths.

- [ ] **Step 1: Update README quick install**

Add an Alpine/postmarketOS dependency block and install command using `PREFIX=/usr LIBDIR=/usr/lib`.

- [ ] **Step 2: Update installation guide**

Add Alpine/postmarketOS package names and a recommended system install section for `/usr/lib`.

- [ ] **Step 3: Update troubleshooting paths**

Show Alpine/postmarketOS verification paths alongside Debian multiarch paths.

- [ ] **Step 4: Update changelog**

Add an Unreleased documentation entry for Alpine/postmarketOS installation notes.

- [ ] **Step 5: Verify docs**

Run:

```sh
grep -R "apk add\|postmarketOS\|LIBDIR=/usr/lib" README.md docs CHANGELOG.md
```

Expected: all new Alpine/postmarketOS instructions are discoverable.

- [ ] **Step 6: Commit docs**

Run:

```sh
git add README.md docs/INSTALL.md docs/TROUBLESHOOTING.md CHANGELOG.md
git commit -m "Document Alpine postmarketOS installation"
```

