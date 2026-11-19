#!/usr/bin/env python3
import os
import math
import cairo
import subprocess

TARGET_DIR = "/home/anisur/miqu-project/miqu/miquland/assets/icons/miqu-cursors"
CURSORS_DIR = os.path.join(TARGET_DIR, "cursors")
TMP_PNG_DIR = "/tmp/miqu_cursor_build"

os.makedirs(CURSORS_DIR, exist_ok=True)
os.makedirs(TMP_PNG_DIR, exist_ok=True)

# Write index.theme
index_theme_path = os.path.join(TARGET_DIR, "index.theme")
with open(index_theme_path, "w") as f:
    f.write("[Icon Theme]\nName=Miqu\nComment=Miqu Desktop Environment Official Cursor Theme\nInherits=core\n")

SIZES = [
    (24, 32, 1.0),
    (32, 48, 1.5),
    (48, 64, 2.0)
]

def render_shadow_and_stroke(ctx, draw_path_fn, scale):
    # Shadow
    ctx.save()
    ctx.translate(1.0 * scale, 1.5 * scale)
    ctx.set_source_rgba(0, 0, 0, 0.40)
    ctx.set_line_width(2.8 * scale)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    draw_path_fn(ctx, scale)
    ctx.stroke_preserve()
    ctx.fill()
    ctx.restore()

    # White border
    ctx.save()
    ctx.set_source_rgba(1.0, 1.0, 1.0, 1.0)
    ctx.set_line_width(2.0 * scale)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    draw_path_fn(ctx, scale)
    ctx.stroke()
    ctx.restore()

    # Dark core
    ctx.save()
    ctx.set_source_rgba(0.08, 0.09, 0.12, 1.0)
    draw_path_fn(ctx, scale)
    ctx.fill()
    ctx.restore()

# 1. Default Arrow
def draw_arrow_path(ctx, scale):
    ox, oy = 3.0 * scale, 3.0 * scale
    ctx.move_to(ox, oy)
    ctx.line_to(ox, oy + 17 * scale)
    ctx.line_to(ox + 4.5 * scale, oy + 13 * scale)
    ctx.line_to(ox + 8.5 * scale, oy + 20 * scale)
    ctx.line_to(ox + 10.8 * scale, oy + 18.8 * scale)
    ctx.line_to(ox + 6.8 * scale, oy + 11.8 * scale)
    ctx.line_to(ox + 12.8 * scale, oy + 11.8 * scale)
    ctx.close_path()

def render_default(ctx, scale):
    render_shadow_and_stroke(ctx, draw_arrow_path, scale)

# 2. Pointer Hand
def draw_pointer_path(ctx, scale):
    ox, oy = 8.0 * scale, 2.0 * scale
    # Index finger pointing up
    ctx.move_to(ox, oy + 4 * scale)
    ctx.arc(ox + 2.0 * scale, oy + 2.0 * scale, 2.0 * scale, math.pi, 0)
    ctx.line_to(ox + 4.0 * scale, oy + 11 * scale)
    # Middle finger
    ctx.arc(ox + 6.0 * scale, oy + 8.0 * scale, 2.0 * scale, -math.pi, 0)
    ctx.line_to(ox + 8.0 * scale, oy + 12 * scale)
    # Ring finger
    ctx.arc(ox + 10.0 * scale, oy + 9.0 * scale, 2.0 * scale, -math.pi, 0)
    ctx.line_to(ox + 12.0 * scale, oy + 13 * scale)
    # Pinky finger
    ctx.arc(ox + 13.8 * scale, oy + 11.0 * scale, 1.8 * scale, -math.pi, 0)
    ctx.line_to(ox + 15.6 * scale, oy + 18 * scale)
    # Palm bottom
    ctx.curve_to(ox + 15.6 * scale, oy + 21 * scale, ox + 11 * scale, oy + 23 * scale, ox + 6 * scale, oy + 23 * scale)
    ctx.curve_to(ox + 2 * scale, oy + 23 * scale, ox, oy + 19 * scale, ox, oy + 16 * scale)
    # Thumb
    ctx.arc(ox - 2.0 * scale, oy + 12.0 * scale, 2.0 * scale, 0.5 * math.pi, 1.5 * math.pi)
    ctx.line_to(ox, oy + 11 * scale)
    ctx.line_to(ox, oy + 4 * scale)
    ctx.close_path()

def render_pointer(ctx, scale):
    render_shadow_and_stroke(ctx, draw_pointer_path, scale)

# 3. Text I-Beam
def draw_text_path(ctx, scale):
    cx, cy = 12.0 * scale, 12.0 * scale
    # Top serif
    ctx.move_to(cx - 5 * scale, cy - 8 * scale)
    ctx.line_to(cx + 5 * scale, cy - 8 * scale)
    ctx.line_to(cx + 5 * scale, cy - 6 * scale)
    ctx.line_to(cx + 1.5 * scale, cy - 6 * scale)
    ctx.line_to(cx + 1.5 * scale, cy + 6 * scale)
    ctx.line_to(cx + 5 * scale, cy + 6 * scale)
    ctx.line_to(cx + 5 * scale, cy + 8 * scale)
    ctx.line_to(cx - 5 * scale, cy + 8 * scale)
    ctx.line_to(cx - 5 * scale, cy + 6 * scale)
    ctx.line_to(cx - 1.5 * scale, cy + 6 * scale)
    ctx.line_to(cx - 1.5 * scale, cy - 6 * scale)
    ctx.line_to(cx - 5 * scale, cy - 6 * scale)
    ctx.close_path()

def render_text(ctx, scale):
    render_shadow_and_stroke(ctx, draw_text_path, scale)

# 4. Crosshair
def draw_crosshair_path(ctx, scale):
    cx, cy = 14.0 * scale, 14.0 * scale
    r = 9.0 * scale
    # Cross bars
    ctx.move_to(cx, cy - r)
    ctx.line_to(cx, cy + r)
    ctx.move_to(cx - r, cy)
    ctx.line_to(cx + r, cy)

def render_crosshair(ctx, scale):
    cx, cy = 14.0 * scale, 14.0 * scale
    r = 8.0 * scale
    
    # Shadow
    ctx.save()
    ctx.translate(1.0 * scale, 1.0 * scale)
    ctx.set_source_rgba(0, 0, 0, 0.4)
    ctx.set_line_width(3.0 * scale)
    draw_crosshair_path(ctx, scale)
    ctx.stroke()
    ctx.arc(cx, cy, r, 0, 2 * math.pi)
    ctx.stroke()
    ctx.restore()

    # White outer
    ctx.save()
    ctx.set_source_rgba(1, 1, 1, 1)
    ctx.set_line_width(2.2 * scale)
    draw_crosshair_path(ctx, scale)
    ctx.stroke()
    ctx.arc(cx, cy, r, 0, 2 * math.pi)
    ctx.stroke()
    ctx.restore()

    # Dark core
    ctx.save()
    ctx.set_source_rgba(0.08, 0.09, 0.12, 1)
    ctx.set_line_width(1.2 * scale)
    draw_crosshair_path(ctx, scale)
    ctx.stroke()
    ctx.arc(cx, cy, r, 0, 2 * math.pi)
    ctx.stroke()
    ctx.restore()

# 5. Move / 4-way arrow
def draw_move_path(ctx, scale):
    cx, cy = 15.0 * scale, 15.0 * scale
    # 4-way cross
    for angle in [0, 0.5 * math.pi, math.pi, 1.5 * math.pi]:
        ctx.save()
        ctx.translate(cx, cy)
        ctx.rotate(angle)
        ctx.move_to(0, -11 * scale)
        ctx.line_to(3.5 * scale, -7.5 * scale)
        ctx.line_to(1.5 * scale, -7.5 * scale)
        ctx.line_to(1.5 * scale, -2 * scale)
        ctx.line_to(-1.5 * scale, -2 * scale)
        ctx.line_to(-1.5 * scale, -7.5 * scale)
        ctx.line_to(-3.5 * scale, -7.5 * scale)
        ctx.close_path()
        ctx.restore()

def render_move(ctx, scale):
    render_shadow_and_stroke(ctx, draw_move_path, scale)

# 6. EW Resize (Horizontal double arrow)
def draw_ew_resize_path(ctx, scale):
    cx, cy = 15.0 * scale, 15.0 * scale
    for angle in [0, math.pi]:
        ctx.save()
        ctx.translate(cx, cy)
        ctx.rotate(angle)
        ctx.move_to(11 * scale, 0)
        ctx.line_to(7.5 * scale, 3.5 * scale)
        ctx.line_to(7.5 * scale, 1.5 * scale)
        ctx.line_to(0, 1.5 * scale)
        ctx.line_to(0, -1.5 * scale)
        ctx.line_to(7.5 * scale, -1.5 * scale)
        ctx.line_to(7.5 * scale, -3.5 * scale)
        ctx.close_path()
        ctx.restore()

def render_ew_resize(ctx, scale):
    render_shadow_and_stroke(ctx, draw_ew_resize_path, scale)

# 7. NS Resize (Vertical double arrow)
def draw_ns_resize_path(ctx, scale):
    cx, cy = 15.0 * scale, 15.0 * scale
    for angle in [0.5 * math.pi, 1.5 * math.pi]:
        ctx.save()
        ctx.translate(cx, cy)
        ctx.rotate(angle)
        ctx.move_to(11 * scale, 0)
        ctx.line_to(7.5 * scale, 3.5 * scale)
        ctx.line_to(7.5 * scale, 1.5 * scale)
        ctx.line_to(0, 1.5 * scale)
        ctx.line_to(0, -1.5 * scale)
        ctx.line_to(7.5 * scale, -1.5 * scale)
        ctx.line_to(7.5 * scale, -3.5 * scale)
        ctx.close_path()
        ctx.restore()

def render_ns_resize(ctx, scale):
    render_shadow_and_stroke(ctx, draw_ns_resize_path, scale)

# 8. NWSE Resize (Diagonal \)
def draw_nwse_resize_path(ctx, scale):
    cx, cy = 15.0 * scale, 15.0 * scale
    for angle in [0.75 * math.pi, -0.25 * math.pi]:
        ctx.save()
        ctx.translate(cx, cy)
        ctx.rotate(angle)
        ctx.move_to(11 * scale, 0)
        ctx.line_to(7.5 * scale, 3.5 * scale)
        ctx.line_to(7.5 * scale, 1.5 * scale)
        ctx.line_to(0, 1.5 * scale)
        ctx.line_to(0, -1.5 * scale)
        ctx.line_to(7.5 * scale, -1.5 * scale)
        ctx.line_to(7.5 * scale, -3.5 * scale)
        ctx.close_path()
        ctx.restore()

def render_nwse_resize(ctx, scale):
    render_shadow_and_stroke(ctx, draw_nwse_resize_path, scale)

# 9. NESW Resize (Diagonal /)
def draw_nesw_resize_path(ctx, scale):
    cx, cy = 15.0 * scale, 15.0 * scale
    for angle in [0.25 * math.pi, -0.75 * math.pi]:
        ctx.save()
        ctx.translate(cx, cy)
        ctx.rotate(angle)
        ctx.move_to(11 * scale, 0)
        ctx.line_to(7.5 * scale, 3.5 * scale)
        ctx.line_to(7.5 * scale, 1.5 * scale)
        ctx.line_to(0, 1.5 * scale)
        ctx.line_to(0, -1.5 * scale)
        ctx.line_to(7.5 * scale, -1.5 * scale)
        ctx.line_to(7.5 * scale, -3.5 * scale)
        ctx.close_path()
        ctx.restore()

def render_nesw_resize(ctx, scale):
    render_shadow_and_stroke(ctx, draw_nesw_resize_path, scale)

# 10. Grab (Open hand)
def draw_grab_path(ctx, scale):
    ox, oy = 5.0 * scale, 6.0 * scale
    ctx.move_to(ox + 3 * scale, oy + 4 * scale)
    ctx.line_to(ox + 3 * scale, oy + 1 * scale)
    ctx.arc(ox + 4.5 * scale, oy + 1 * scale, 1.5 * scale, math.pi, 0)
    ctx.line_to(ox + 6 * scale, oy + 4 * scale)
    ctx.line_to(ox + 6 * scale, oy + 0 * scale)
    ctx.arc(ox + 7.5 * scale, oy + 0 * scale, 1.5 * scale, math.pi, 0)
    ctx.line_to(ox + 9 * scale, oy + 4 * scale)
    ctx.line_to(ox + 9 * scale, oy + 1 * scale)
    ctx.arc(ox + 10.5 * scale, oy + 1 * scale, 1.5 * scale, math.pi, 0)
    ctx.line_to(ox + 12 * scale, oy + 5 * scale)
    ctx.arc(ox + 13.5 * scale, oy + 3 * scale, 1.5 * scale, math.pi, 0)
    ctx.line_to(ox + 15 * scale, oy + 11 * scale)
    ctx.curve_to(ox + 15 * scale, oy + 16 * scale, ox + 11 * scale, oy + 18 * scale, ox + 6 * scale, oy + 18 * scale)
    ctx.curve_to(ox + 1 * scale, oy + 18 * scale, ox, oy + 15 * scale, ox, oy + 12 * scale)
    ctx.arc(ox - 1 * scale, oy + 8 * scale, 1.5 * scale, 0.5 * math.pi, 1.5 * math.pi)
    ctx.line_to(ox + 3 * scale, oy + 6 * scale)
    ctx.close_path()

def render_grab(ctx, scale):
    render_shadow_and_stroke(ctx, draw_grab_path, scale)

# 11. Grabbing (Closed fist)
def draw_grabbing_path(ctx, scale):
    ox, oy = 6.0 * scale, 6.0 * scale
    ctx.arc(ox + 8 * scale, oy + 8 * scale, 7 * scale, 0, 2 * math.pi)

def render_grabbing(ctx, scale):
    ox, oy = 6.0 * scale, 6.0 * scale
    cx, cy = ox + 8 * scale, oy + 8 * scale
    r = 7.0 * scale
    
    # Shadow
    ctx.save()
    ctx.translate(1.0 * scale, 1.5 * scale)
    ctx.set_source_rgba(0, 0, 0, 0.40)
    ctx.arc(cx, cy, r + 1.0 * scale, 0, 2 * math.pi)
    ctx.fill()
    ctx.restore()

    # White border
    ctx.set_source_rgba(1, 1, 1, 1)
    ctx.arc(cx, cy, r + 1.0 * scale, 0, 2 * math.pi)
    ctx.fill()

    # Dark core
    ctx.set_source_rgba(0.08, 0.09, 0.12, 1)
    ctx.arc(cx, cy, r - 0.5 * scale, 0, 2 * math.pi)
    ctx.fill()

# 12. Not Allowed (Slashed Circle)
def render_not_allowed(ctx, scale):
    cx, cy = 14.0 * scale, 14.0 * scale
    r = 9.0 * scale

    # Shadow
    ctx.save()
    ctx.translate(1.0 * scale, 1.5 * scale)
    ctx.set_source_rgba(0, 0, 0, 0.40)
    ctx.arc(cx, cy, r + 1.0 * scale, 0, 2 * math.pi)
    ctx.fill()
    ctx.restore()

    # White border
    ctx.set_source_rgba(1, 1, 1, 1)
    ctx.arc(cx, cy, r + 1.0 * scale, 0, 2 * math.pi)
    ctx.fill()

    # Red ring
    ctx.set_source_rgba(0.88, 0.15, 0.25, 1) # Miqu Crimson
    ctx.set_line_width(2.8 * scale)
    ctx.arc(cx, cy, r - 0.5 * scale, 0, 2 * math.pi)
    ctx.stroke()

    # Slash
    ctx.move_to(cx - 5.5 * scale, cy - 5.5 * scale)
    ctx.line_to(cx + 5.5 * scale, cy + 5.5 * scale)
    ctx.stroke()

# 13. Wait Spinner (Animated 8 frames)
def render_wait_frame(ctx, scale, frame_idx, num_frames=8):
    cx, cy = 15.0 * scale, 15.0 * scale
    r = 8.5 * scale
    
    # Background ring
    ctx.set_source_rgba(0.08, 0.09, 0.12, 0.25)
    ctx.set_line_width(2.8 * scale)
    ctx.arc(cx, cy, r, 0, 2 * math.pi)
    ctx.stroke()

    # Spinning segment (Miqu Accent Indigo/Purple)
    angle_start = (frame_idx / num_frames) * 2 * math.pi
    angle_end = angle_start + 1.1 * math.pi
    
    # Shadow
    ctx.save()
    ctx.translate(1.0 * scale, 1.0 * scale)
    ctx.set_source_rgba(0, 0, 0, 0.35)
    ctx.set_line_width(3.2 * scale)
    ctx.arc(cx, cy, r, angle_start, angle_end)
    ctx.stroke()
    ctx.restore()

    # White border behind active arc
    ctx.set_source_rgba(1, 1, 1, 1)
    ctx.set_line_width(3.6 * scale)
    ctx.arc(cx, cy, r, angle_start, angle_end)
    ctx.stroke()

    # Core active arc
    ctx.set_source_rgba(0.39, 0.40, 0.95, 1) # Modern Indigo #6366F1
    ctx.set_line_width(2.4 * scale)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.arc(cx, cy, r, angle_start, angle_end)
    ctx.stroke()

# 14. Progress (Arrow + mini spinner)
def render_progress_frame(ctx, scale, frame_idx, num_frames=8):
    render_default(ctx, scale * 0.9)
    # Mini spinner at bottom right
    cx, cy = 18.0 * scale, 18.0 * scale
    r = 5.0 * scale
    angle_start = (frame_idx / num_frames) * 2 * math.pi
    angle_end = angle_start + 1.0 * math.pi

    ctx.set_source_rgba(1, 1, 1, 1)
    ctx.set_line_width(2.6 * scale)
    ctx.arc(cx, cy, r, angle_start, angle_end)
    ctx.stroke()

    ctx.set_source_rgba(0.39, 0.40, 0.95, 1)
    ctx.set_line_width(1.8 * scale)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)
    ctx.arc(cx, cy, r, angle_start, angle_end)
    ctx.stroke()

# 15. Help (Arrow + question mark)
def render_help(ctx, scale):
    render_default(ctx, scale * 0.9)
    # Question mark bubble
    cx, cy = 18.0 * scale, 16.0 * scale
    
    ctx.set_source_rgba(1, 1, 1, 1)
    ctx.arc(cx, cy, 4.5 * scale, 0, 2 * math.pi)
    ctx.fill()

    ctx.set_source_rgba(0.08, 0.09, 0.12, 1)
    ctx.set_font_size(6.0 * scale)
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.move_to(cx - 1.8 * scale, cy + 2.2 * scale)
    ctx.show_text("?")

# Define all cursors and their hotspots (at nominal 24px)
CURSORS = [
    {
        "name": "default",
        "render": lambda ctx, sc: render_default(ctx, sc),
        "hx": 3, "hy": 3,
        "animated": False
    },
    {
        "name": "pointer",
        "render": lambda ctx, sc: render_pointer(ctx, sc),
        "hx": 10, "hy": 3,
        "animated": False
    },
    {
        "name": "text",
        "render": lambda ctx, sc: render_text(ctx, sc),
        "hx": 12, "hy": 12,
        "animated": False
    },
    {
        "name": "crosshair",
        "render": lambda ctx, sc: render_crosshair(ctx, sc),
        "hx": 14, "hy": 14,
        "animated": False
    },
    {
        "name": "move",
        "render": lambda ctx, sc: render_move(ctx, sc),
        "hx": 15, "hy": 15,
        "animated": False
    },
    {
        "name": "ew-resize",
        "render": lambda ctx, sc: render_ew_resize(ctx, sc),
        "hx": 15, "hy": 15,
        "animated": False
    },
    {
        "name": "ns-resize",
        "render": lambda ctx, sc: render_ns_resize(ctx, sc),
        "hx": 15, "hy": 15,
        "animated": False
    },
    {
        "name": "nwse-resize",
        "render": lambda ctx, sc: render_nwse_resize(ctx, sc),
        "hx": 15, "hy": 15,
        "animated": False
    },
    {
        "name": "nesw-resize",
        "render": lambda ctx, sc: render_nesw_resize(ctx, sc),
        "hx": 15, "hy": 15,
        "animated": False
    },
    {
        "name": "grab",
        "render": lambda ctx, sc: render_grab(ctx, sc),
        "hx": 12, "hy": 12,
        "animated": False
    },
    {
        "name": "grabbing",
        "render": lambda ctx, sc: render_grabbing(ctx, sc),
        "hx": 14, "hy": 14,
        "animated": False
    },
    {
        "name": "not-allowed",
        "render": lambda ctx, sc: render_not_allowed(ctx, sc),
        "hx": 14, "hy": 14,
        "animated": False
    },
    {
        "name": "help",
        "render": lambda ctx, sc: render_help(ctx, sc),
        "hx": 3, "hy": 3,
        "animated": False
    },
    {
        "name": "wait",
        "render": lambda ctx, sc, fr: render_wait_frame(ctx, sc, fr, 8),
        "hx": 15, "hy": 15,
        "animated": True,
        "frames": 8,
        "delay": 60
    },
    {
        "name": "progress",
        "render": lambda ctx, sc, fr: render_progress_frame(ctx, sc, fr, 8),
        "hx": 3, "hy": 3,
        "animated": True,
        "frames": 8,
        "delay": 60
    }
]

# Compile each cursor
print(f"Generating miqu-cursors in {CURSORS_DIR}...")
for c in CURSORS:
    name = c["name"]
    cfg_lines = []
    
    if c.get("animated", False):
        num_frames = c["frames"]
        delay = c["delay"]
        for frame_idx in range(num_frames):
            for nominal_sz, canvas_sz, scale in SIZES:
                hx = int(round(c["hx"] * scale))
                hy = int(round(c["hy"] * scale))
                png_path = f"{TMP_PNG_DIR}/{name}_{nominal_sz}_f{frame_idx}.png"
                surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, canvas_sz, canvas_sz)
                ctx = cairo.Context(surface)
                c["render"](ctx, scale, frame_idx)
                surface.write_to_png(png_path)
                cfg_lines.append(f"{nominal_sz} {hx} {hy} {png_path} {delay}\n")
    else:
        for nominal_sz, canvas_sz, scale in SIZES:
            hx = int(round(c["hx"] * scale))
            hy = int(round(c["hy"] * scale))
            png_path = f"{TMP_PNG_DIR}/{name}_{nominal_sz}.png"
            surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, canvas_sz, canvas_sz)
            ctx = cairo.Context(surface)
            c["render"](ctx, scale)
            surface.write_to_png(png_path)
            cfg_lines.append(f"{nominal_sz} {hx} {hy} {png_path}\n")

    cfg_file = f"{TMP_PNG_DIR}/{name}.cfg"
    with open(cfg_file, "w") as f:
        f.writelines(cfg_lines)

    out_cursor = os.path.join(CURSORS_DIR, name)
    subprocess.run(["xcursorgen", cfg_file, out_cursor], check=True)
    print(f"  ✓ {name} ({os.path.getsize(out_cursor)} bytes)")

# Symbolic links for compatibility across X11, Wayland, CSS3, and GTK
SYMLINKS = {
    # Default arrow
    "arrow": "default",
    "left_ptr": "default",
    "top_left_arrow": "default",
    # Pointer hand
    "hand": "pointer",
    "hand2": "pointer",
    "pointing_hand": "pointer",
    # Text I-beam
    "xterm": "text",
    "ibeam": "text",
    # Crosshair
    "cross": "crosshair",
    "tcross": "crosshair",
    # Move
    "fleur": "move",
    "all-scroll": "move",
    "size-all": "move",
    # Grab / Grabbing
    "openhand": "grab",
    "closedhand": "grabbing",
    "dnd-move": "grabbing",
    # Not Allowed
    "forbidden": "not-allowed",
    "circle": "not-allowed",
    "crossed_circle": "not-allowed",
    # Wait / Progress
    "watch": "wait",
    "left_ptr_watch": "progress",
    "08e8e1c95fe2fc01f976f1e063a24ccd": "progress",
    # Help
    "whats_this": "help",
    "question_arrow": "help",
    # Resizing
    "col-resize": "ew-resize",
    "h_double_arrow": "ew-resize",
    "size_hor": "ew-resize",
    "split_h": "ew-resize",
    "e-resize": "ew-resize",
    "w-resize": "ew-resize",
    "left_side": "ew-resize",
    "right_side": "ew-resize",

    "row-resize": "ns-resize",
    "v_double_arrow": "ns-resize",
    "size_ver": "ns-resize",
    "split_v": "ns-resize",
    "n-resize": "ns-resize",
    "s-resize": "ns-resize",
    "top_side": "ns-resize",
    "bottom_side": "ns-resize",

    "top_left_corner": "nwse-resize",
    "bottom_right_corner": "nwse-resize",
    "nw-resize": "nwse-resize",
    "se-resize": "nwse-resize",
    "bd_double_arrow": "nwse-resize",
    "size_bdiag": "nwse-resize",

    "top_right_corner": "nesw-resize",
    "bottom_left_corner": "nesw-resize",
    "ne-resize": "nesw-resize",
    "sw-resize": "nesw-resize",
    "fd_double_arrow": "nesw-resize",
    "size_fdiag": "nesw-resize",
}

print("Creating compatibility symlinks...")
for alias, target in SYMLINKS.items():
    link_path = os.path.join(CURSORS_DIR, alias)
    if os.path.lexists(link_path):
        os.remove(link_path)
    os.symlink(target, link_path)

print(f"Done! miqu-cursors created successfully in {TARGET_DIR}.")
