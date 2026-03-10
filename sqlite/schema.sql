-- SQLite schema for NFC Card Game
-- Run once to initialize cardgame.db: sqlite3 cardgame.db < sqlite/schema.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cards (
    card_id    TEXT PRIMARY KEY,
    name       TEXT NOT NULL,
    cost       INTEGER NOT NULL DEFAULT 0,
    type       TEXT NOT NULL,
    rules_text TEXT,
    data       TEXT
);

CREATE TABLE IF NOT EXISTS nfc_tags (
    uid     TEXT PRIMARY KEY,          -- uppercase hex UID, e.g. "04A1B2C3"
    card_id TEXT NOT NULL REFERENCES cards(card_id)
);
