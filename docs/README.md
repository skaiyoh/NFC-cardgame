# Documentation Index

These docs replace the deleted `.planning/` tree. They were checked against the current source tree and local command output on 2026-04-03.

## Local Verification Completed

- `make clean cardgame`
- `make preview`
- `make biome_preview`
- `make card_enroll`
- `make test`
- `pkg-config --modversion raylib` -> `5.5.0`
- `sqlite3` queries against the checked-in `cardgame.db`
- `sqlite3` queries against a fresh temporary database created from `sqlite/schema.sql` and `sqlite/seed.sql`

## Start Here

- `project-status.md` - what is implemented, what is partial, and what is still stubbed
- `CODEMAPS/architecture.md` - runtime ownership and system boundaries
- `CODEMAPS/testing.md` - automated coverage and verification notes
- `CODEMAPS/known-issues.md` - current mismatches and open risks

## Code Maps

- `CODEMAPS/architecture.md`
- `CODEMAPS/backend.md`
- `CODEMAPS/frontend.md`
- `CODEMAPS/data.md`
- `CODEMAPS/dependencies.md`
- `CODEMAPS/structure.md`
- `CODEMAPS/testing.md`
- `CODEMAPS/integrations.md`
- `CODEMAPS/conventions.md`
- `CODEMAPS/known-issues.md`
