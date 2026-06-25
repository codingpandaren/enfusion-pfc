# Propeller Flight Core - Documentation

Developer documentation for the **Propeller Flight Core (PFC)** Arma Reforger mod, built with
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/) and hosted on GitHub Pages.

**Live site:** https://codingpandaren.github.io/enfusion-pfc/

## How it deploys

Every push to `main` triggers `.github/workflows/deploy.yml`, which builds the site and publishes it to the
`gh-pages` branch. **You don't need anything installed locally - just edit Markdown in `docs/` and push.**

One-time GitHub setup after the first push:

1. Push this repo to `main`. The workflow runs and creates the `gh-pages` branch.
2. Repo **Settings → Pages → Build and deployment → Source: Deploy from a branch**.
3. Select branch **`gh-pages`**, folder **`/ (root)`**, save.

The site is live at the URL above within a minute or two.

## Editing

- All content lives in `docs/` as Markdown.
- Navigation is defined by the `nav:` block in `mkdocs.yml`.
- Drop screenshots into `docs/assets/images/` and reference them with `![alt](assets/images/name.png)`.
  Pages currently mark where screenshots help with a "📷 Screenshot" callout - replace those as you capture them.

## Previewing locally (optional)

Requires Python 3:

```bash
pip install -r requirements.txt
mkdocs serve
```

Then open <http://127.0.0.1:8000>. The preview live-reloads as you edit.
