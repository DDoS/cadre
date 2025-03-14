import sqlite3
import re
from pathlib import Path, PurePosixPath
from datetime import datetime


sqlite3.register_adapter(datetime, lambda dt: dt.isoformat())
sqlite3.register_converter('datetime', lambda dt: datetime.fromisoformat(dt.decode()))

sqlite3.register_adapter(PurePosixPath, lambda path: str(path))
sqlite3.register_converter('path', lambda path: PurePosixPath(path.decode()))


identifier_pattern = re.compile('^[A-Za-z_][A-Za-z0-9_]*$')


def open(path: Path) -> sqlite3.Connection:
    db = sqlite3.connect(path, detect_types=sqlite3.PARSE_COLNAMES, timeout=60)
    with db:
        db.execute('PRAGMA foreign_keys = ON')
    return db


def setup(path: Path | str):
    db = open(path)
    with db:
        db.execute('CREATE TABLE IF NOT EXISTS refresh_jobs(id INTEGER PRIMARY KEY, identifier TEXT NOT NULL UNIQUE, '
                   'display_name TEXT NOT NULL, hostname TEXT NOT NULL, schedule TEXT NOT NULL, enabled INTEGER NOT NULL, '
                   'filter TEXT NOT NULL, "order" TEXT NOT NULL, affiche_options_json TEXT, post_command_id TEXT NOT NULL)')
        db.execute('CREATE TABLE IF NOT EXISTS collections(id INTEGER PRIMARY KEY, identifier TEXT NOT NULL UNIQUE, '
                   'display_name TEXT NOT NULL, schedule TEXT NOT NULL, enabled INTEGER NOT NULL, '
                   'class_name TEXT NOT NULL, settings_json TEXT)')
        db.execute('CREATE TABLE IF NOT EXISTS photos(id INTEGER PRIMARY KEY, collection_id INTEGER NOT NULL REFERENCES '
                   'collections(id) ON DELETE CASCADE, cycle_count INTEGER NOT NULL DEFAULT 0, display_date TEXT, '
                   'format TEXT, width INTEGER, height INTEGER, favorite INTEGER, capture_date TEXT)')

        if not db.execute("SELECT COUNT(*) FROM pragma_table_info('refresh_jobs') WHERE name='post_command_id'").fetchone()[0]:
            db.execute("ALTER TABLE refresh_jobs ADD COLUMN post_command_id TEXT NOT NULL DEFAULT ''")

    db.close()


def validate_identifier(identifier: str) -> bool:
    return identifier_pattern.fullmatch(identifier) is not None
