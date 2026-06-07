import os
import re
from contextlib import asynccontextmanager

import cv2
import easyocr
import numpy as np
from fastapi import FastAPI, Form, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.templating import Jinja2Templates
from starlette.middleware.sessions import SessionMiddleware

import database

reader = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global reader
    database.init_db()
    reader = easyocr.Reader(["en"], gpu=False)
    yield


app = FastAPI(lifespan=lifespan)
app.add_middleware(
    SessionMiddleware,
    secret_key=os.getenv("SECRET_KEY", "plat-change-this-secret-in-production"),
)
templates = Jinja2Templates(directory="templates")


# ── Auth helper ───────────────────────────────────────────────

def logged_in(request: Request):
    return request.session.get("username")


def login_redirect():
    return RedirectResponse(url="/login", status_code=303)


# ── Auth routes ───────────────────────────────────────────────

@app.get("/login", response_class=HTMLResponse)
async def login_page(request: Request):
    if logged_in(request):
        return RedirectResponse(url="/", status_code=303)
    return templates.TemplateResponse(
        request=request, name="login.html", context={"error": None}
    )


@app.post("/login")
async def login(request: Request,
                username: str = Form(...),
                password: str = Form(...)):
    user = database.get_user(username.strip())
    if not user or not database.verify_password(password, user["password_hash"]):
        return templates.TemplateResponse(
            request=request, name="login.html",
            context={"error": "Invalid username or password."}
        )
    request.session["username"]   = user["username"]
    request.session["first_name"] = user["first_name"]
    return RedirectResponse(url="/", status_code=303)


@app.get("/logout")
async def logout(request: Request):
    request.session.clear()
    return RedirectResponse(url="/login", status_code=303)


# ── Dashboard ─────────────────────────────────────────────────

@app.get("/", response_class=HTMLResponse)
async def dashboard(request: Request):
    if not logged_in(request):
        return login_redirect()
    cars = database.get_all_cars()
    return templates.TemplateResponse(
        request=request, name="index.html",
        context={
            "cars":       cars,
            "username":   request.session.get("username"),
            "first_name": request.session.get("first_name"),
        }
    )


# ── Car routes ────────────────────────────────────────────────

@app.post("/cars/add")
async def add_car(
    request:      Request,
    plate:        str = Form(...),
    matrix:       str = Form(""),
    first_name:   str = Form(""),
    last_name:    str = Form(""),
    manufacturer: str = Form(""),
    model:        str = Form(""),
    colour:       str = Form(""),
):
    if not logged_in(request):
        return login_redirect()
    database.add_car(
        plate.strip().upper(), matrix.strip(), first_name.strip(),
        last_name.strip(), manufacturer.strip(), model.strip(), colour.strip(),
    )
    return RedirectResponse(url="/", status_code=303)


@app.post("/cars/edit/{original_plate}")
async def edit_car(
    request:        Request,
    original_plate: str,
    plate:          str = Form(...),
    matrix:         str = Form(""),
    first_name:     str = Form(""),
    last_name:      str = Form(""),
    manufacturer:   str = Form(""),
    model:          str = Form(""),
    colour:         str = Form(""),
):
    if not logged_in(request):
        return login_redirect()
    database.update_car(
        original_plate, plate.strip().upper(), matrix.strip(),
        first_name.strip(), last_name.strip(), manufacturer.strip(),
        model.strip(), colour.strip(),
    )
    return RedirectResponse(url="/", status_code=303)


@app.post("/cars/delete/{plate}")
async def delete_car(request: Request, plate: str):
    if not logged_in(request):
        return login_redirect()
    database.delete_car(plate)
    return RedirectResponse(url="/", status_code=303)


# ── Plate recognition (ESP32 — no auth needed) ────────────────

@app.post("/recognize")
async def recognize(request: Request):
    contents = await request.body()
    np_arr = np.frombuffer(contents, np.uint8)
    frame  = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

    if frame is None:
        return {"plate": None, "verified": False}

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    results = reader.readtext(thresh)

    plate = None
    for (_, text, confidence) in results:
        cleaned = re.sub(r"[^A-Z0-9]", "", text.upper())
        if (2 <= len(cleaned) <= 8
                and re.search(r"[A-Z]", cleaned)
                and re.search(r"[0-9]", cleaned)
                and confidence > 0.5):
            plate = cleaned
            break

    if not plate:
        return {"plate": None, "verified": False}

    verified = database.is_verified(plate)
    return {"plate": plate, "verified": verified}
