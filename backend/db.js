// db.js — initializes SQLite DB and prepares statements
const Database = require('better-sqlite3');
const db = new Database('chat.sqlite');

// Create tables if not exists
db.exec(`
CREATE TABLE IF NOT EXISTS users (
  id TEXT PRIMARY KEY,
  username TEXT UNIQUE NOT NULL,
  last_seen INTEGER
);

CREATE TABLE IF NOT EXISTS messages (
  id TEXT PRIMARY KEY,
  sender TEXT NOT NULL,
  recipient TEXT NOT NULL,
  content TEXT NOT NULL,
  timestamp INTEGER NOT NULL,
  FOREIGN KEY(sender) REFERENCES users(username),
  FOREIGN KEY(recipient) REFERENCES users(username)
);
`);

module.exports = db;