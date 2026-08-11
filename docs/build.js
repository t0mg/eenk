#!/usr/bin/env node
/**
 * docs/build.js — Minimal static site generator for the eenk documentation site.
 *
 * Usage:
 *   node build.js           Build to dist/
 *   node build.js --watch   Rebuild on file change (dev mode, requires chokidar)
 *
 * Dependencies: marked (npm install in docs/)
 */

'use strict';

const fs = require('fs');
const path = require('path');
const https = require('https');
const { marked } = require('marked');

// ── Paths ──────────────────────────────────────────────────────────────────
const DOCS_DIR = __dirname;
const REPO_ROOT = path.resolve(DOCS_DIR, '..');
const DIST_DIR = path.join(DOCS_DIR, 'dist');
const TEMPLATE = path.join(DOCS_DIR, 'template.html');
const CONFIG_PATH = path.join(DOCS_DIR, 'site-config.json');
const FLASHER_SRC = path.join(REPO_ROOT, 'tools/flasher');
const DEVICE_MANAGER_SRC = path.join(REPO_ROOT, 'tools/device-manager');
const ASSETS_SRC = path.join(DOCS_DIR, 'assets');
const ASSETS_EXTRA_SRC = path.join(DOCS_DIR, 'assets-webonly');

// ── Eenky Credits Resolver ─────────────────────────────────────────────────
async function ensureEenkyCredits() {
  const targetPath = path.join(DOCS_DIR, 'pages', 'eenky-credits.md');
  const localSourcePath = path.join(REPO_ROOT, 'tools', 'eenky', 'credits.md');

  // 1. Check if local tools/eenky/credits.md exists
  if (fs.existsSync(localSourcePath)) {
    try {
      const content = fs.readFileSync(localSourcePath, 'utf8');
      ensureDir(path.dirname(targetPath));
      fs.writeFileSync(targetPath, content, 'utf8');
      console.log('  ✓ Using local tools/eenky/credits.md');
      return;
    } catch (e) {
      console.warn(`  ⚠ Could not read local tools/eenky/credits.md: ${e.message}`);
    }
  }

  // 2. Fetch remote credits from GitHub API
  const url = 'https://api.github.com/repos/t0mg/eenky/contents/credits.md';
  const token = process.env.GITHUB_TOKEN || '';

  try {
    const fetchGithub = (fetchUrl) => new Promise((resolve, reject) => {
      const headers = {
        'User-Agent': 'eenk-docs-builder',
        'Accept': 'application/vnd.github.raw+json'
      };
      if (token) headers['Authorization'] = `Bearer ${token}`;

      https.get(fetchUrl, { headers }, (res) => {
        if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
          return fetchGithub(res.headers.location).then(resolve, reject);
        }
        if (res.statusCode !== 200) {
          return reject(new Error(`HTTP ${res.statusCode}`));
        }
        const chunks = [];
        res.on('data', c => chunks.push(c));
        res.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
        res.on('error', reject);
      }).on('error', reject);
    });

    const content = await fetchGithub(url);
    ensureDir(path.dirname(targetPath));
    fs.writeFileSync(targetPath, content, 'utf8');
    console.log('  ✓ Fetched remote eenky credits from GitHub API');
  } catch (err) {
    if (fs.existsSync(targetPath)) {
      console.log('  ✓ Using existing pages/eenky-credits.md');
    } else {
      console.warn(`  ⚠ Could not fetch remote eenky credits: ${err.message}`);
    }
  }
}

// ── Config ─────────────────────────────────────────────────────────────────
const config = JSON.parse(fs.readFileSync(CONFIG_PATH, 'utf8'));
const { site, nav } = config;

// ── marked configuration & heading slugger ──────────────────────────────────
let slugCounts = {};

function resetSlugCounts() {
  slugCounts = {};
}

function slugify(text) {
  const plainText = text.replace(/<[^>]+>/g, '').trim();
  let slug = plainText
    .toLowerCase()
    .replace(/[^\w\s-]/g, '')
    .replace(/\s+/g, '-');
  if (!slug) slug = 'section';

  if (slugCounts[slug]) {
    const count = slugCounts[slug];
    slugCounts[slug]++;
    slug = `${slug}-${count}`;
  } else {
    slugCounts[slug] = 1;
  }
  return slug;
}

marked.use({
  gfm: true,
  breaks: false,
  renderer: {
    heading(text, level, raw) {
      const id = slugify(raw || text);
      return `<h${level} id="${id}">${text}</h${level}>\n`;
    }
  }
});

// ── Helpers ────────────────────────────────────────────────────────────────
function ensureDir(dir) {
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
}

function copyDir(src, dest) {
  ensureDir(dest);
  for (const entry of fs.readdirSync(src, { withFileTypes: true })) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      copyDir(srcPath, destPath);
    } else {
      fs.copyFileSync(srcPath, destPath);
    }
  }
}

/** Compute a relative path from a page's output dir back to the dist root. */
function assetRoot(outFile) {
  const depth = outFile.split('/').length - 1;
  return depth === 0 ? './' : '../'.repeat(depth);
}

/** Compute the canonical URL for a page. */
function canonicalUrl(outFile) {
  const clean = outFile === 'index.html' ? '' : outFile;
  return `${site.baseUrl}/${clean}`;
}

/** Build the <nav> items HTML for a given active page id. */
function buildNavItems(activeId) {
  return nav.map(item => {
    const isActive = item.id === activeId;
    const classes = [
      isActive ? 'active' : ''
    ].filter(Boolean).join(' ');
    const href = item.out;
    return `<li><a href="${href}"${classes ? ` class="${classes}"` : ''} aria-current="${isActive ? 'page' : 'false'}">${item.title}</a></li>`;
  }).join('\n      ');
}

/** Read a Markdown file and convert to HTML. */
function mdToHtml(filePath) {
  const raw = fs.readFileSync(filePath, 'utf8');
  return marked.parse(raw);
}

/** Resolve all nav href paths relative to output location. */
function resolveNavHrefs(navHtml, assetRootPrefix) {
  // Replace plain out paths with correct relative paths
  return nav.reduce((html, item) => {
    return html.replaceAll(`href="${item.out}"`, `href="${assetRootPrefix}${item.out}"`);
  }, navHtml);
}

// ── Build ──────────────────────────────────────────────────────────────────
async function build() {
  console.log('📦 Building eenk documentation site...');
  ensureDir(DIST_DIR);

  await ensureEenkyCredits();

  const templateHtml = fs.readFileSync(TEMPLATE, 'utf8');

  // 1. Copy assets/
  copyDir(ASSETS_SRC, path.join(DIST_DIR, 'assets'));
  copyDir(ASSETS_EXTRA_SRC, path.join(DIST_DIR, 'assets'));
  console.log('  ✓ Copied assets/ (with extras)');

  // 2. Copy flasher/ and device-manager/ (portable modules, live in repo root)
  if (fs.existsSync(FLASHER_SRC)) {
    copyDir(FLASHER_SRC, path.join(DIST_DIR, 'flasher'));
    const flasherIndex = path.join(DIST_DIR, 'flasher', 'index.html');
    if (fs.existsSync(flasherIndex)) {
      fs.renameSync(flasherIndex, path.join(DIST_DIR, 'flasher', 'app.html'));
    }
    console.log('  ✓ Copied flasher/');
  } else {
    console.warn('  ⚠ flasher/ directory not found — skipping');
  }

  if (fs.existsSync(DEVICE_MANAGER_SRC)) {
    copyDir(DEVICE_MANAGER_SRC, path.join(DIST_DIR, 'device-manager'));
    const dmIndex = path.join(DIST_DIR, 'device-manager', 'index.html');
    if (fs.existsSync(dmIndex)) {
      fs.renameSync(dmIndex, path.join(DIST_DIR, 'device-manager', 'app.html'));
    }
    console.log('  ✓ Copied device-manager/');
  } else {
    console.warn('  ⚠ device-manager/ directory not found — skipping');
  }

  // 3. Build each page
  for (const page of nav) {
    const outFile = page.out;
    const assetPfx = assetRoot(outFile);
    const outPath = path.join(DIST_DIR, outFile);

    ensureDir(path.dirname(outPath));

    let bodyHtml = '';
    let bodyClass = '';

    if (page.external) {
      bodyClass = ' class="is-module"';
      bodyHtml = `<iframe src="app.html" title="${page.title}" class="module-iframe" allow="serial"></iframe>`;
      console.log(`  ✓ External module wrapper: ${page.id} (${outFile})`);
    } else {
      resetSlugCounts();
      // Read primary markdown
      let mdContent = fs.readFileSync(path.join(DOCS_DIR, page.src), 'utf8');

      // Append any extra markdown files (e.g. WritingForEenk.md into eenky page)
      if (page.importAppend) {
        for (const rel of page.importAppend) {
          const appendPath = path.resolve(DOCS_DIR, rel);
          if (fs.existsSync(appendPath)) {
            const appendMd = fs.readFileSync(appendPath, 'utf8');
            mdContent += '\n\n<div class="page-content">\n\n' + appendMd + '\n\n</div>';
          } else {
            console.warn(`  ⚠ importAppend file not found: ${appendPath}`);
          }
        }
      }

      bodyHtml = marked.parse(mdContent);
    }

    // Build nav items with correct relative hrefs
    const rawNavItems = buildNavItems(page.id);
    const navItems = resolveNavHrefs(rawNavItems, assetPfx);

    // Inject into template
    const description = site.description;
    let html = templateHtml
      .replaceAll('{{BODY_CLASS}}', bodyClass)
      .replaceAll('{{PAGE_TITLE}}', page.title)
      .replaceAll('{{PAGE_DESCRIPTION}}', description)
      .replaceAll('{{CANONICAL_URL}}', canonicalUrl(outFile))
      .replaceAll('{{ASSET_ROOT}}', assetPfx)
      .replaceAll('{{SITE_ROOT}}', assetPfx)
      .replaceAll('{{GITHUB_URL}}', site.githubUrl)
      .replaceAll('{{NAV_ITEMS}}', navItems)
      .replaceAll('{{PAGE_CONTENT}}', bodyHtml);

    fs.writeFileSync(outPath, html, 'utf8');
    console.log(`  ✓ Built ${outFile}`);
  }

  console.log(`\n✅ Site built to dist/ — ${nav.length} pages processed`);
}

// ── Entry ──────────────────────────────────────────────────────────────────
build().catch(err => {
  console.error('❌ Build failed:', err);
  process.exit(1);
});
