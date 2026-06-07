import os
import sqlite3

import bcrypt

DATABASE_PATH = os.getenv("DATABASE_PATH", "data/cars.db")


def get_connection():
    dir_path = os.path.dirname(DATABASE_PATH)
    if dir_path:
        os.makedirs(dir_path, exist_ok=True)
    return sqlite3.connect(DATABASE_PATH)


def init_db():
    with get_connection() as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS cars (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                plate        TEXT UNIQUE NOT NULL,
                matrix       TEXT,
                first_name   TEXT,
                last_name    TEXT,
                manufacturer TEXT,
                model        TEXT,
                colour       TEXT,
                created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS users (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                username     TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                first_name   TEXT,
                last_name    TEXT,
                phone        TEXT,
                created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.commit()
    _create_default_user()


# ── Users ─────────────────────────────────────────────────────

def hash_password(password: str) -> str:
    return bcrypt.hashpw(password.encode(), bcrypt.gensalt()).decode()


def verify_password(plain: str, hashed: str) -> bool:
    return bcrypt.checkpw(plain.encode(), hashed.encode())


def _create_default_user():
    with get_connection() as conn:
        exists = conn.execute(
            "SELECT 1 FROM users WHERE username = ?", ("admin",)
        ).fetchone()
        if not exists:
            conn.execute("""
                INSERT INTO users (username, password_hash, first_name, last_name, phone)
                VALUES (?, ?, ?, ?, ?)
            """, ("admin", hash_password("admin@1234"), "Admin", "User", ""))
            conn.commit()


def get_user(username: str):
    with get_connection() as conn:
        row = conn.execute("""
            SELECT id, username, password_hash, first_name, last_name, phone
            FROM users WHERE username = ?
        """, (username,)).fetchone()
    if not row:
        return None
    return {
        "id":        row[0],
        "username":  row[1],
        "password_hash": row[2],
        "first_name": row[3] or "",
        "last_name":  row[4] or "",
        "phone":      row[5] or "",
    }




# ── Cars ──────────────────────────────────────────────────────

def get_all_cars():
    with get_connection() as conn:
        rows = conn.execute("""
            SELECT id, plate, matrix, first_name, last_name,
                   manufacturer, model, colour, created_at
            FROM cars ORDER BY created_at DESC
        """).fetchall()
    return [
        {
            "id":           r[0],
            "plate":        r[1],
            "matrix":       r[2] or "",
            "first_name":   r[3] or "",
            "last_name":    r[4] or "",
            "manufacturer": r[5] or "",
            "model":        r[6] or "",
            "colour":       r[7] or "",
            "created_at":   r[8],
        }
        for r in rows
    ]


def add_car(plate: str, matrix: str = "", first_name: str = "",
            last_name: str = "", manufacturer: str = "",
            model: str = "", colour: str = "") -> bool:
    try:
        with get_connection() as conn:
            conn.execute("""
                INSERT INTO cars (plate, matrix, first_name, last_name,
                                  manufacturer, model, colour)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (plate.upper(), matrix, first_name, last_name,
                  manufacturer, model, colour))
            conn.commit()
        return True
    except sqlite3.IntegrityError:
        return False


def update_car(original_plate: str, plate: str, matrix: str = "",
               first_name: str = "", last_name: str = "",
               manufacturer: str = "", model: str = "",
               colour: str = "") -> bool:
    try:
        with get_connection() as conn:
            cursor = conn.execute("""
                UPDATE cars SET plate=?, matrix=?, first_name=?, last_name=?,
                                manufacturer=?, model=?, colour=?
                WHERE plate=?
            """, (plate.upper(), matrix, first_name, last_name,
                  manufacturer, model, colour, original_plate.upper()))
            conn.commit()
        return cursor.rowcount > 0
    except sqlite3.IntegrityError:
        return False


def delete_car(plate: str) -> bool:
    with get_connection() as conn:
        cursor = conn.execute("DELETE FROM cars WHERE plate = ?", (plate.upper(),))
        conn.commit()
    return cursor.rowcount > 0


def is_verified(plate: str) -> bool:
    with get_connection() as conn:
        row = conn.execute(
            "SELECT 1 FROM cars WHERE plate = ?", (plate.upper(),)
        ).fetchone()
    return row is not None
