# Site

[terraverse.arclabs.it](https://www.terraverse.arclabs.it/): the landing page, the docs (the mod API is designed here first) and
the wiki. Nuxt with @nuxt/content, prerendered to a static site.

```
app/        pages, layouts, components
content/    docs/ and wiki/, in Markdown: content/docs is served at [/docs](https://www.terraverse.arclabs.it/docs)
public/     static files
build/      the Dockerfile and nginx.conf it's served with
scripts/    deploy, verify, and bake (the wiki's baked images)
```

## Developing

```bash
bun run dev         # localhost, with hot reload (or `bun dev:site` from the root)
bun run typecheck
bun run lint        # oxlint
bun run build       # nuxt generate, into .output/public
bun run check       # typecheck, the install script's check, then a full build
```

Deploys go through `arc deploy site`, which runs `scripts/deploy.ts`: the static build and
`install.sh` to Cloud Run. It takes no secrets.
