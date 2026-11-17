# The FM-1 test installer (the site's tools/test-installer/)

`index.html` is a page that installs any FM-1 firmware package (`.fwsc`) the user chooses, for trying a build (a
GitHub Actions run's dev package) on an FM-1 before it is released. It is fm1-base's own web installer with a file
picker in place of the site's package; it shows the package's identity and SHA-256 before Install, and the FM-1 must
come back reporting that identity.

It is made in gio-tools (`fm1_test_installer/make_page.py`, with its browser test), from fm1-base `c8e0599`, and
copied here unchanged. `tools/make_pages.py` puts it on the site at `tools/test-installer/`, unlisted (no link from
the site's pages, `noindex`). Rebuild it in gio-tools and copy it again when `base/` moves.
