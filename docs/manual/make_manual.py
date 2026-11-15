#!/usr/bin/env python3
# Bassline YF-21 - Copyright (C) 2026 LexLongcoding
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Builds docs/BasslineYF21-User-Manual.pdf.

Control names, ranges, defaults and menu choices are read straight from
Source/Parameters.h, so the manual stays in step with the plugin.
Requires: pip install reportlab     Run from anywhere: python3 docs/manual/make_manual.py
"""
import os, re
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table,
                                TableStyle, Image, PageBreak, KeepTogether, NextPageTemplate, CondPageBreak)
from reportlab.graphics.shapes import Drawing, Circle, Line, Path, Polygon, PolyLine
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "docs", "BasslineYF21-User-Manual.pdf")
IMG = os.path.join(ROOT, "docs", "images")
VERSION = re.search(r"project\(BasslineYF21 VERSION ([0-9.]+)\)", open(os.path.join(ROOT, "CMakeLists.txt")).read()).group(1)

# ---------------------------------------------------------------- fonts & colours (same as Decu6000)
FONTS = os.path.join(ROOT, "Resources", "Fonts")
pdfmetrics.registerFont(TTFont("DecuDisplay", os.path.join(FONTS, "DecuDisplay-Bold.ttf")))
pdfmetrics.registerFont(TTFont("DecuBlack", os.path.join(FONTS, "DecuDisplay-Black.ttf")))
pdfmetrics.registerFont(TTFont("DecuMono", os.path.join(FONTS, "ShareTechMono-Regular.ttf")))

BG     = colors.HexColor("#12102B")
PANEL  = colors.HexColor("#2A2150")
ORANGE = colors.HexColor("#FF7A1A")
PINK   = colors.HexColor("#FF2E93")
CYAN   = colors.HexColor("#3DE8FF")
INK    = colors.HexColor("#1D1838")
SOFT   = colors.HexColor("#F3F1FB")
RULE   = colors.HexColor("#D8D2FF")
MUTED  = colors.HexColor("#5E5885")
TEAL   = colors.HexColor("#1F6F86")
MAGENTA = colors.HexColor("#8E1D57")

S = {
    "h1":   ParagraphStyle("h1", fontName="DecuDisplay", fontSize=20, leading=24, textColor=INK, spaceBefore=4, spaceAfter=10),
    "h2":   ParagraphStyle("h2", fontName="DecuDisplay", fontSize=13, leading=17, textColor=PINK, spaceBefore=12, spaceAfter=5),
    "h3":   ParagraphStyle("h3", fontName="Helvetica-Bold", fontSize=10.5, leading=14, textColor=INK, spaceBefore=8, spaceAfter=3),
    "body": ParagraphStyle("body", fontName="Helvetica", fontSize=9.6, leading=13.6, textColor=INK, spaceAfter=6),
    "bul":  ParagraphStyle("bul", fontName="Helvetica", fontSize=9.6, leading=13.4, textColor=INK, leftIndent=14,
                           bulletIndent=3, spaceAfter=2.5),
    "cell": ParagraphStyle("cell", fontName="Helvetica", fontSize=8.6, leading=11, textColor=INK),
    "cellb":ParagraphStyle("cellb", fontName="Helvetica-Bold", fontSize=8.6, leading=11, textColor=INK),
    "head": ParagraphStyle("head", fontName="DecuMono", fontSize=8.6, leading=11, textColor=colors.white),
    "cap":  ParagraphStyle("cap", fontName="Helvetica-Oblique", fontSize=8.4, leading=11, textColor=MUTED,
                           alignment=TA_CENTER, spaceBefore=3, spaceAfter=10),
    "note": ParagraphStyle("note", fontName="Helvetica", fontSize=9, leading=12.6, textColor=INK),
    "toc":  ParagraphStyle("toc", fontName="Helvetica", fontSize=10.5, leading=18, textColor=INK),
}

def P(t, s="body"): return Paragraph(t, S[s])
def M(t): return f"<font face='DecuMono'>{t}</font>"
def bullets(items): return [Paragraph(i, S["bul"], bulletText="•") for i in items]

def note(text, colour=CYAN):
    t = Table([[Paragraph(text, S["note"])]], colWidths=[6.5 * inch])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), SOFT), ("LINEBEFORE", (0, 0), (0, -1), 3, colour),
                           ("LEFTPADDING", (0, 0), (-1, -1), 10), ("RIGHTPADDING", (0, 0), (-1, -1), 10),
                           ("TOPPADDING", (0, 0), (-1, -1), 7), ("BOTTOMPADDING", (0, 0), (-1, -1), 7)]))
    return t

def table(rows, widths, header=True, accent=PANEL):
    data = []
    for r, row in enumerate(rows):
        style = "head" if (header and r == 0) else "cell"
        data.append([c if not isinstance(c, str) else Paragraph(c, S[style]) for c in row])
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    st = [("VALIGN", (0, 0), (-1, -1), "MIDDLE"), ("LINEBELOW", (0, 0), (-1, -1), 0.4, RULE),
          ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
          ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5)]
    if header:
        st += [("BACKGROUND", (0, 0), (-1, 0), accent)]
    for r in range(1 if header else 0, len(rows)):
        if r % 2 == 0:
            st.append(("BACKGROUND", (0, r), (-1, r), SOFT))
    t.setStyle(TableStyle(st))
    return t

def shot(name, width=6.5 * inch, caption=None):
    path = os.path.join(IMG, name)
    from reportlab.lib.utils import ImageReader
    iw, ih = ImageReader(path).getSize()
    out = [Image(path, width=width, height=width * ih / iw)]
    if caption: out.append(P(caption, "cap"))
    return KeepTogether(out)

# ---------------------------------------------------------------- step symbols, drawn like the plugin draws them
def symbol(kind, size=18):
    d = Drawing(size, size)
    s = float(size)
    col = {"rest": INK, "slide": TEAL, "accent": PINK, "up": ORANGE, "down": ORANGE}[kind]
    if kind == "rest":
        d.add(Circle(s / 2, s / 2, s * 0.32, strokeColor=col, strokeWidth=s * 0.09, fillColor=None))
        d.add(Line(s * 0.27, s * 0.27, s * 0.73, s * 0.73, strokeColor=col, strokeWidth=s * 0.09))
    elif kind == "slide":
        y = s * 0.38
        p = Path(strokeColor=col, strokeWidth=s * 0.1, fillColor=None, strokeLineCap=1)
        p.moveTo(s * 0.12, y); p.curveTo(s * 0.35, s * 0.85, s * 0.65, s * 0.85, s * 0.88, y)
        d.add(p)
        d.add(Circle(s * 0.12, y, s * 0.07, fillColor=col, strokeColor=None))
        d.add(Circle(s * 0.88, y, s * 0.07, fillColor=col, strokeColor=None))
    elif kind == "accent":
        d.add(PolyLine([s * 0.3, s * 0.8, s * 0.75, s * 0.5, s * 0.3, s * 0.2], strokeColor=col, strokeWidth=s * 0.12))
    elif kind == "up":
        d.add(Polygon([s * 0.5, s * 0.82, s * 0.82, s * 0.25, s * 0.18, s * 0.25], fillColor=col, strokeColor=None))
    elif kind == "down":
        d.add(Polygon([s * 0.18, s * 0.75, s * 0.82, s * 0.75, s * 0.5, s * 0.18], fillColor=col, strokeColor=None))
    return d

# ---------------------------------------------------------------- parameters from source
src = open(os.path.join(ROOT, "Source", "Parameters.h")).read()
CHOICE_CONSTS = {m[0]: m[1] for m in re.findall(r'constexpr const char\*\s*(\w+)\s*=\s*"([^"]*)"', src, re.S)}
NUM = r'([-0-9.]+)f?'
SPEC_RE = re.compile(r'\{\s*"(\w+)",\s*"([^"]+)",\s*"([^"]+)",\s*' + NUM + r',\s*' + NUM + r',\s*' + NUM + r',\s*' + NUM
                     + r',\s*U::(\w+),\s*([^,]+?),\s*(true|false)\s*\}')
SPECS = {}
for m in SPEC_RE.findall(src):
    ch = m[8].strip()
    choices = None
    if ch.startswith('"'): choices = ch.strip('"').split("|")
    elif ch in CHOICE_CONSTS: choices = CHOICE_CONSTS[ch].split("|")
    SPECS[m[0]] = dict(id=m[0], name=m[1], knob=m[2], lo=float(m[3]), hi=float(m[4]), default=float(m[6]),
                       unit=m[7], choices=choices, patch=m[9] == "true")
assert len(SPECS) > 50, "couldn't read Parameters.h"

def signed(v): return f"{'+' if v > 0 else ''}{v}"

def fmt(v, s):
    u = s["unit"]
    if u == "Percent":   return f"{round(v * 100)}%"
    if u == "Bipolar":   return f"{signed(round(v * 100))}%"
    if u == "Hz":        return f"{v / 1000:.0f} kHz" if v >= 1000 and v % 1000 == 0 else (f"{v / 1000:.1f} kHz" if v >= 1000 else (f"{v:.2f} Hz" if v < 20 else f"{round(v)} Hz"))
    if u == "Time":      return (f"{v * 1000:.0f} ms" if v * 1000 >= 10 else f"{v * 1000:.1f} ms".replace(".0 ms", " ms")) if v < 1 else f"{v:.2f} s".replace(".00 s", " s")
    if u == "Semitones": return f"{signed(round(v))} st" if float(v).is_integer() else f"{v} st"
    if u == "Cents":     return f"{signed(round(v))} ct"
    if u == "Octave":    return f"{signed(round(v))} oct"
    if u == "Bpm":       return f"{v:g} BPM"
    if u == "Swing":     return f"{round(50 + v * 25)}%"
    if u == "Toggle":    return "ON" if v >= 0.5 else "OFF"
    if u == "Choice":    return s["choices"][int(v)]
    if u == "Note":      return f"{trig_name(int(v))} ({int(v)})"
    return f"{v:g}"

def rng(s):
    if s["unit"] == "Toggle": return "ON / OFF"
    if s["unit"] == "Choice": return ", ".join(s["choices"]) if len(s["choices"]) <= 5 else f"{len(s['choices'])} choices"
    return f"{fmt(s['lo'], s)} to {fmt(s['hi'], s)}"

def controls(ids_desc, accent=PANEL):
    rows = [["CONTROL", "WHAT IT DOES", "RANGE", "DEFAULT"]]
    for pid, desc in ids_desc:
        s = SPECS[pid]
        rows.append([f"<b>{s['knob']}</b>", desc, rng(s), fmt(s["default"], s)])
    return table(rows, [0.85 * inch, 3.55 * inch, 1.3 * inch, 0.8 * inch], accent=accent)

def trig_name(note):
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    return f"{names[note % 12]}{note // 12 - 2}"

# ---------------------------------------------------------------- page decoration
def on_cover(c, doc):
    w, h = letter
    c.setFillColor(BG); c.rect(0, 0, w, h, stroke=0, fill=1)
    c.setFillColor(colors.Color(0.24, 0.91, 1.0, alpha=0.035))
    for y in range(0, int(h), 3): c.rect(0, y, w, 1, stroke=0, fill=1)
    bw = w - 1.5 * inch
    c.setFillColor(ORANGE); c.rect(0.75 * inch, h - 3.05 * inch, bw * 0.5, 3, stroke=0, fill=1)
    c.setFillColor(PINK);   c.rect(0.75 * inch + bw * 0.5, h - 3.05 * inch, bw * 0.333, 3, stroke=0, fill=1)
    c.setFillColor(CYAN);   c.rect(0.75 * inch + bw * 0.833, h - 3.05 * inch, bw * 0.167, 3, stroke=0, fill=1)
    c.setFont("DecuBlack", 46)
    word = "BASSLINE "
    c.setFillColor(PINK);   c.drawString(0.75 * inch, h - 2.2 * inch, word)
    c.setFillColor(ORANGE); c.drawString(0.75 * inch + pdfmetrics.stringWidth(word, "DecuBlack", 46), h - 2.2 * inch, "YF-21")
    c.setFillColor(colors.HexColor("#D8D2FF")); c.setFont("DecuMono", 13)
    c.drawString(0.78 * inch, h - 2.65 * inch, "ACID BASS SYNTHESIZER // 3 OSC + 16 STEP SEQUENCER")
    c.setFont("DecuDisplay", 26); c.setFillColor(colors.white)
    c.drawString(0.75 * inch, h - 3.75 * inch, "USER MANUAL")
    c.setFont("DecuMono", 11); c.setFillColor(CYAN)
    c.drawString(0.78 * inch, h - 4.1 * inch, f"VERSION {VERSION}  //  VST3 + STANDALONE  //  WINDOWS + LINUX")
    iw = w - 1.5 * inch; ih = iw * 930 / 1440
    c.drawImage(os.path.join(IMG, "editor.png"), 0.75 * inch, 1.3 * inch, iw, ih)
    c.setStrokeColor(colors.HexColor("#3A3170")); c.setLineWidth(1); c.rect(0.75 * inch, 1.3 * inch, iw, ih)
    c.setFont("DecuMono", 9); c.setFillColor(colors.HexColor("#8A83B8"))
    c.drawString(0.78 * inch, 0.8 * inch, "FREE & OPEN SOURCE  //  GNU AGPL v3.0 OR LATER  //  GITHUB.COM/LEXLONGCODING/BASSLINEYF21")

def on_page(c, doc):
    w, h = letter
    c.setFillColor(BG); c.rect(0, h - 0.42 * inch, w, 0.42 * inch, stroke=0, fill=1)
    c.setFont("DecuBlack", 11); c.setFillColor(PINK); c.drawString(0.75 * inch, h - 0.28 * inch, "BASSLINE ")
    c.setFillColor(ORANGE); c.drawString(0.75 * inch + pdfmetrics.stringWidth("BASSLINE ", "DecuBlack", 11), h - 0.28 * inch, "YF-21")
    c.setFont("DecuMono", 8.5); c.setFillColor(colors.HexColor("#D8D2FF"))
    c.drawRightString(w - 0.75 * inch, h - 0.28 * inch, "USER MANUAL  //  v" + VERSION)
    c.setFillColor(ORANGE); c.rect(0, h - 0.45 * inch, w * 0.5, 2, stroke=0, fill=1)
    c.setFillColor(PINK); c.rect(w * 0.5, h - 0.45 * inch, w * 0.333, 2, stroke=0, fill=1)
    c.setFillColor(CYAN); c.rect(w * 0.833, h - 0.45 * inch, w * 0.167, 2, stroke=0, fill=1)
    c.setFont("DecuMono", 8.5); c.setFillColor(MUTED)
    c.drawCentredString(w / 2, 0.45 * inch, str(doc.page))

# ---------------------------------------------------------------- content
story = [NextPageTemplate("body"), PageBreak()]

TOC = ["1  Welcome", "2  Installing", "3  Quick start", "4  The editor", "5  Oscillators", "6  LFO",
       "7  Filter, accent and slide", "8  Amp envelope, effects and master", "9  The sequencer",
       "10  Recording patterns", "11  Using it in a DAW", "12  Tempo and sync", "13  MIDI learn",
       "14  Patches and patterns", "15  Sound design tips", "16  Troubleshooting", "17  Credits and license"]
story += [P("Contents", "h1")] + [P(t, "toc") for t in TOC]
story += [Spacer(1, 14), note("<b>In a hurry?</b> Sections 3 and 11 get you from loading the plugin to recording "
                              "acid lines in your DAW. The rest is reference you can dip into.", ORANGE), PageBreak()]

# 1 welcome
story += [P("1  Welcome", "h1"),
          P("Bassline YF-21 is a monophonic acid bass synthesizer with a built-in 16-step sequencer. It follows the "
            "classic single-oscillator, squelchy-filter, slide-and-accent machine sound, and adds the extras of a modern "
            "plugin: three oscillators, wavetables, an LFO, a wave folder, distortion and a sequencer you can record into "
            "live or step by step."),
          P("It is the sister of the Decu6000 drum synthesizer and shares its look: same panels, knobs, fonts, LCDs, "
            "patch system and MIDI learn."),
          P("What's inside", "h2")]
story += bullets([
    "<b>3 oscillators</b> - each with on/off, wave, octave, semitone, fine tune, shape and level, and its own "
    "button to patch the LFO in. 4 classic waves plus 8 digital wavetables.",
    "<b>LFO</b> - 7 waves, free or tempo-synced, routed to any oscillator's pitch and shape and to the filter cutoff.",
    "<b>Filter</b> - three modes (a thin, squelchy 303-style diode filter, a 24 dB ladder and a 12 dB state-variable "
    "filter) with cutoff, resonance, envelope amount, decay, accent, drive, key tracking and slide time.",
    "<b>Real slide and accent</b> - slides glide without retriggering; accents punch harder and push the filter "
    "further on every accent in a row.",
    "<b>Amp envelope, wave folder and distortion</b>, an oscilloscope and an output meter.",
    "<b>Sequencer</b> - 16 steps with REST, SLIDE, ACCENT, OCTAVE UP and OCTAVE DOWN per step, 16 pattern slots, "
    "live and step recording, swing, gate, length 1-16 and a random acid-line generator.",
    "<b>Two ways to play from a DAW</b> - PATTERN mode (hold a key to play a pattern slot, other keys transpose it) "
    "and FREE mode (a normal synth: overlapping notes slide, hard notes accent).",
    "<b>Tempo display</b> with host sync, <b>MIDI learn</b> on every control, and libraries of "
    "<b>patches</b> (sounds) and <b>patterns</b> (notes).",
])
story += [PageBreak()]

# 2 installing
story += [P("2  Installing", "h1"),
          P("Bassline YF-21 comes as a <b>VST3 plugin</b> for your DAW (Reaper, Bitwig, Ableton Live, Cubase, FL Studio, "
            "Ardour...) and a <b>Standalone</b> app you can play without a DAW."),
          P("Windows (64-bit)", "h2")]
story += bullets([
    f"<b>VST3:</b> copy the whole {M('Bassline YF-21.vst3')} folder to {M('C:&#92;Program Files&#92;Common Files&#92;VST3&#92;')} "
    "(Windows asks for administrator permission). Rescan plugins in your DAW - it appears as an instrument.",
    f"<b>Standalone:</b> double-click {M('Bassline YF-21.exe')}. No installation needed.",
    "The first time, Windows may show <i>\"Windows protected your PC\"</i> because the app isn't signed with a paid "
    "certificate. Click <b>More info</b>, then <b>Run anyway</b>.",
])
story += [P("Linux (x86_64)", "h2")]
story += bullets([
    f"<b>VST3:</b> copy {M('Bassline YF-21.vst3')} to {M('~/.vst3/')} and rescan in your DAW.",
    f"<b>Standalone:</b> run {M('./&quot;Bassline YF-21&quot;')} (if needed, first {M('chmod +x')} it). ALSA and JACK "
    f"are supported; with PipeWire, {M('pw-jack')} works too.",
])
story += [P("Setting up the Standalone", "h2"),
          P("Click <b>Options</b> (top-left of the window), then <b>Audio/MIDI Settings...</b> Pick your audio output "
            "(a 128-256 sample buffer is a good start) and tick your keyboard or controller under <i>Active MIDI inputs</i>."),
          Spacer(1, 6),
          note("In the Standalone there is no DAW to follow, so the sequencer runs on the tempo shown in the header. "
               "Press <b>PLAY</b> to hear the selected pattern.", ORANGE),
          PageBreak()]

# 3 quick start
story += [P("3  Quick start", "h1")]
story += bullets([
    "<b>1. Load it</b> on an instrument track in your DAW, or open the Standalone.",
    "<b>2. Hear a pattern.</b> Press <b>PLAY</b> in the sequencer panel. Slot 1 is already filled with a factory "
    "line - you'll hear it straight away.",
    "<b>3. Twist the classic knobs.</b> While it plays, turn <b>CUTOFF</b>, <b>RESO</b>, <b>ENV MOD</b>, "
    "<b>DECAY</b> and <b>ACCENT</b> in the filter panel. That's the acid sound.",
    "<b>4. Try other slots.</b> Click the numbered slot buttons 1-16 above the grid; slots 1-8 start with factory patterns.",
    "<b>5. Try patches.</b> Click the arrows beside the patch LCD in the header to step through the factory sounds.",
    "<b>6. Make your own line.</b> Click <b>RANDOM</b> for a new acid line, or edit the grid: drag notes up and down, "
    "click the modifier rows to add slides, accents and rests.",
    "<b>7. Use it in your DAW.</b> Stop PLAY, then in the piano roll draw a long <b>C0</b> note: slot 1 plays for as "
    "long as the note lasts (section 11).",
])
story += [Spacer(1, 6), shot("synth_panels.png", caption="The synth panels. Top: three oscillators, LFO, filter. "
                                                         "Bottom: amp envelope, wave folder, distortion, scope, master."),
          PageBreak()]

# 4 editor
story += [P("4  The editor", "h1"),
          P("Everything is on one page: the header, the synth panels in two rows, and the sequencer underneath."),
          shot("editor.png", caption="The whole editor, playing slot 1 (the cyan column is the current step).")]
story += [P("The header", "h2")]
story += bullets([
    "<b>Tempo display</b> - the current tempo in BPM. <font color='#1F6F86'><b>HOST</b></font> means it is following "
    "your DAW, <b>INT</b> means its own tempo. Drag it up or down (Shift for fine steps), scroll, or double-click "
    "for 125 BPM. The pink dot flashes on every beat while a pattern plays.",
    "<b>SYNC</b> - follow the DAW's tempo and position (section 12).",
    f"<b>Patch LCD</b> - the current sound, e.g. {M('PATCH 02 // CLASSIC ACID')}. A {M('*')} means you changed "
    "it since loading. While you turn a knob it shows the value. Click it for the patch menu; the arrows step "
    "through patches.",
    "<b>PANIC</b> - all notes off: stops the sequencer (and PLAY) and silences the synth immediately. Use it if "
    "a note ever hangs.",
    "<b>MIDI LEARN</b> - assign controller knobs to the plugin's controls (section 13).",
])
story += [P("Handy everywhere", "h2")]
story += bullets([
    "<b>Double-click</b> a knob to reset it to its default.",
    "<b>Right-click</b> any knob, button or menu to MIDI-learn it, clear its controller, or reset it.",
    "<b>Hover</b> over a control to see its full name.",
    "<b>Resize</b> the window from its bottom-right corner; it keeps its shape and remembers its size.",
])
story += [PageBreak()]

# 5 oscillators
WAVE_DESC = [
    ("SAW", "The classic acid wave: bright and buzzy.", "Blends in a second saw half a cycle later: thicker, then an octave up."),
    ("SQUARE", "Hollow and woody - the other classic acid wave.", "Pulse width, from 50% (square) to a thin 5% pulse."),
    ("TRIANGLE", "Soft and round, few harmonics.", "Skews the triangle into a ramp: brighter."),
    ("SINE", "Pure fundamental - a sub bass.", "Phase distortion: bends the sine into a saw-like wave."),
    ("WT FORMANT", "Vocal formants.", "Walks the vowels A - E - I - O - U."),
    ("WT ORGAN", "Drawbar organ registrations.", "From a mellow two-bar sound to full organ."),
    ("WT SYNC", "Hard-sync sweep.", "Raises the synced pitch: the classic tearing sync sound."),
    ("WT FOLD", "Wavefolded sine.", "Folds harder: from smooth to metallic."),
    ("WT BITS", "Bit-crushed sine.", "From 6 bits down to 1 bit, with stepped sample-and-hold."),
    ("WT PHASE", "Phase-distortion resonance.", "Sweeps a resonant peak upward, like a filter sweep inside the wave."),
    ("WT GLASS", "2-operator FM.", "Raises the FM depth: from sine to glassy bell."),
    ("WT HARMONIC", "A band of harmonics over a quiet fundamental.", "Sweeps the band upward: a hollow, singing tone."),
]
story += [P("5  Oscillators", "h1"),
          P("Three identical oscillators are mixed into the filter. OSC 1 starts switched on with a saw - the classic "
            "single-oscillator acid voice. Switch on OSC 2 and OSC 3 to stack intervals, detune, sub octaves or wavetables."),
          controls([("osc1_on", "Switches the oscillator on or off (the button in its title bar)."),
                    ("osc1_wave", "The waveform: 4 classic shapes and 8 wavetables (table below)."),
                    ("osc1_oct", "Octave shift."),
                    ("osc1_semi", "Semitone shift - for fifths (+7), octaves plus intervals, etc."),
                    ("osc1_fine", "Fine tune in cents. A few cents between two oscillators gives a thick, detuned sound."),
                    ("osc1_shape", "Changes the wave - what it does depends on the wave (table below)."),
                    ("osc1_level", "Volume of this oscillator going into the filter."),
                    ("osc1_lfo", "Patches the LFO into this oscillator's pitch and shape (depths are set on the LFO panel).")]),
          P("OSC 2 starts as a square, 7 cents sharp; OSC 3 as a sine one octave down. Both start switched off.", "cap"),
          P("Waves and what SHAPE does", "h2"),
          table([["WAVE", "SOUND", "SHAPE"]] + [[f"<b>{a}</b>", b, c] for a, b, c in WAVE_DESC],
                [1.15 * inch, 2.4 * inch, 2.95 * inch]),
          Spacer(1, 6),
          note("The small screen at the bottom of each oscillator draws the actual waveform, so you can see what SHAPE "
               "is doing. Wavetables are generated by the plugin and band-limited, so high notes stay clean."),
          PageBreak()]

# 6 LFO
story += [P("6  LFO", "h1"),
          P("One LFO (low-frequency oscillator) for movement. It reaches the oscillators you <b>patch</b> it to - press "
            "the <b>LFO</b> button on each oscillator - and the filter cutoff directly. The pink light in the LFO's title "
            "bar shows it moving."),
          controls([("lfo_wave", "Shape of the movement: smooth (triangle, sine), ramps (saw up / down), on-off (square), "
                                 "random steps (sample+hold) or smooth random wandering."),
                    ("lfo_rate", "Speed in Hz when SYNC is off."),
                    ("lfo_sync", "Locks the LFO to the tempo. When the DAW is playing it also lines up with the bar."),
                    ("lfo_div", "LFO cycle length when synced: from 4 bars down to 1/32 notes, including dotted (.) and triplet (T)."),
                    ("lfo_pitch", "How far the LFO bends the pitch of patched oscillators. Small amounts give vibrato; 12 st gives octave sweeps."),
                    ("lfo_shape", "How much the LFO moves the SHAPE of patched oscillators - pulse-width modulation, wavetable scanning."),
                    ("lfo_cutoff", "How much the LFO moves the filter cutoff, up to 3 octaves either way. Works without any oscillator patched.")],
                   accent=MAGENTA),
          Spacer(1, 6),
          note("<b>Try:</b> OSC 1 = WT FORMANT, its LFO button on, LFO SHAPE 80%, SYNC on, DIV 1 BAR - the bass "
               "\"talks\" once per bar.", PINK),
          PageBreak()]

# 7 filter
story += [P("7  Filter, accent and slide", "h1"),
          P("The filter is where the acid sound lives. CUTOFF, RESO, ENV MOD, DECAY and ACCENT are the five classic "
            "controls; DRIVE, KEY TRK, SLIDE and the MODE menu are extras."),
          controls([("flt_mode", "<b>303 DIODE</b>: thin, squelchy resonance that thins out the bass as it rises (~18 dB/oct). "
                                 "<b>24DB LADDER</b>: fatter, keeps the bass under the resonance. <b>12DB SVF</b>: gentler and brighter."),
                    ("flt_cutoff", "Base brightness of the filter."),
                    ("flt_reso", "Resonance: the squelch. Near the top it starts to whistle."),
                    ("flt_envmod", "How far each note's envelope opens the filter above CUTOFF (up to 5 octaves)."),
                    ("flt_decay", "How quickly that envelope closes again. Accented notes always use a short decay."),
                    ("flt_accent", "How hard accented notes hit: louder, plus an extra push to the cutoff (see below)."),
                    ("flt_drive", "Drives the filter input: warmer, then grittier, and the resonance behaves more wildly."),
                    ("flt_keytrk", "Higher notes open the filter more, so high notes don't sound dull."),
                    ("voice_slide", "How long a slide takes to reach the next note.")], accent=TEAL)]
story += [P("Accent", "h2"),
          P("An accented note (the ACCENT modifier in the sequencer, or a note played with velocity 100 or more) is "
            "louder and uses the shortest filter decay. Accent also charges an \"accent sweep\": when several accents "
            "come close together, each one pushes the cutoff a little higher than the last - the rising, screaming "
            "effect of a classic acid line. The ACCENT knob sets how strong all of this is.")]
story += [P("Slide", "h2"),
          P("A slide joins two notes: the gate stays open and the pitch glides into the next note over the SLIDE time, "
            "<i>without</i> restarting the envelopes - so the filter keeps decaying through the glide. In the sequencer "
            "a SLIDE on step 4 means \"slide from step 4 into step 5\". When playing keys, overlapping notes slide.")]
story += [Spacer(1, 4),
          note("<b>Starting point:</b> 303 DIODE, CUTOFF around 400 Hz, RESO 70%, ENV MOD 60%, DECAY 300 ms, ACCENT 70%. "
               "Then ride CUTOFF and ENV MOD while the pattern plays.", CYAN),
          PageBreak()]

# 8 amp, effects, master
story += [P("8  Amp envelope, effects and master", "h1"),
          P("The signal runs: oscillators - filter - amp (VCA) - wave folder - distortion - master."),
          P("Amp envelope", "h2"),
          P("Controls the volume of each note. The defaults behave like the classic machine: an instant start, then the "
            "note fades while held and stops quickly when released. Raise SUSTAIN and RELEASE for pads and drones."),
          controls([("amp_attack", "Fade-in time at the start of a note."),
                    ("amp_decay", "Time to fall to the sustain level."),
                    ("amp_sustain", "Level held while the note is held."),
                    ("amp_release", "Fade-out time after the note ends.")], accent=PANEL),
          CondPageBreak(2.2 * inch),
          P("Wave folder", "h2"),
          P("Folds the signal back on itself when it gets too loud, adding bright, metallic harmonics. Because it comes "
            "after the amp, loud note starts fold more than quiet tails."),
          controls([("fold_amount", "How hard the signal is folded. 0 = off."),
                    ("fold_sym", "Shifts the folding off-centre for different, rougher harmonics."),
                    ("fold_mix", "Blend of folded and clean signal.")], accent=MAGENTA),
          CondPageBreak(2.4 * inch),
          P("Distortion", "h2"),
          controls([("dist_type", "<b>SOFT</b> warm saturation, <b>HARD</b> clipping, <b>FUZZ</b> asymmetric and "
                                  "spitty, <b>TUBE</b> asymmetric and warm, <b>CRUSH</b> bit and sample-rate reduction."),
                    ("dist_drive", "How hard it distorts. 0 = off."),
                    ("dist_tone", "Brightness of the distorted signal."),
                    ("dist_mix", "Blend of distorted and clean signal.")], accent=MAGENTA),
          CondPageBreak(1.8 * inch),
          P("Scope and master", "h2"),
          P("The scope draws the output waveform. The meter shows the output level; a soft limiter only acts above about "
            "-2 dBFS, so normal playing is untouched."),
          controls([("master_volume", "Output volume."),
                    ("master_tune", "Tunes the whole synth up or down by up to a semitone, to match other instruments.")],
                   accent=MAGENTA),
          PageBreak()]

# 9 sequencer
story += [P("9  The sequencer", "h1"),
          shot("sequencer_steprec.png", caption="The sequencer in step record: the yellow column is the record cursor."),
          P("The grid", "h2"),
          P("Each of the 16 columns is a step. The <b>STEP</b> row lights up as the pattern plays. The <b>NOTE</b> row shows "
            "each step's note, from C to the C above (shown as C'), with a small bar showing where it sits in the octave. "
            "Under it are the five modifier rows, each with its own symbol:")]
sym_rows = [["", "MODIFIER", "WHAT IT DOES"],
            [symbol("rest"), "<b>REST</b>", "No note on this step - the gate closes. A rest also shows in the NOTE row."],
            [symbol("slide"), "<b>SLIDE</b>", "Slides from this step into the next one: the note is held and the pitch glides."],
            [symbol("accent"), "<b>ACCENT</b>", "Louder and punchier, with a short, bright filter snap (section 7)."],
            [symbol("up"), "<b>OCT UP</b>", "Plays the note one octave higher. Also shown on the note."],
            [symbol("down"), "<b>OCT DN</b>", "Plays the note one octave lower. Also shown on the note."]]
story += [table(sym_rows, [0.45 * inch, 1.1 * inch, 4.95 * inch])]
story += [P("Editing steps", "h3")]
story += bullets([
    "<b>Note:</b> drag a note up or down, or scroll over it. Right-click for a list of notes. Clicking a rest turns it "
    "into a note. When stopped, clicking a note lets you hear it.",
    "<b>Modifiers:</b> click a cell to switch it on or off. Drag across the row to paint several steps at once. "
    "OCT UP and OCT DN switch each other off.",
    "Steps past the pattern's length are dimmed and don't play.",
])
story += [P("Slots and the toolbar", "h2")]
story += bullets([
    "<b>Slot buttons 1-16</b> - the 16 pattern slots. Click one to show and edit it (orange). The slot that is playing "
    "is outlined in cyan. Right-click a slot to <b>copy</b>, <b>paste</b> or <b>clear</b> it. All 16 slots are saved "
    "with your DAW project.",
    f"<b>Pattern LCD</b> - {M('SLOT 01 // FIRST CONTACT')}: the slot and the name of what's in it ({M('*')} = edited). "
    "Click it to load a pattern from the library into this slot, or to save this slot to the library (section 14).",
    "<b>LEN - / +</b> - pattern length, 1 to 16 steps. Odd lengths make lines that drift against the bar.",
    "<b>&lt; &gt;</b> - shift the whole pattern one step earlier or later.",
    "<b>RANDOM</b> - writes a new random acid line into the slot (keeps the length).",
    "<b>CLEAR</b> - empties the slot.  <b>REST</b> - enters a rest while step recording.",
])
story += [P("Left column", "h2")]
story += bullets([
    "<b>PLAY</b> - plays the selected slot inside the plugin.  <b>REC</b> - record (section 10).",
    "<b>PLAY MODE, PATTERN START, TRIGGER KEYS</b> - how the plugin answers MIDI from your DAW (section 11).",
    f"<b>SWING</b> - delays every second 16th note, from straight ({M('50%')}) to a heavy shuffle ({M('75%')}).",
    "<b>GATE</b> - how long each note is held, as a fraction of a step. Short = staccato, long = legato. Slides always hold.",
    "<b>Keyboard</b> - 13 keys from C to C, like the original. Play it to hear notes and to step record. "
    "<b>KEYS OCT - / +</b> moves it an octave; <b>ACC</b> makes it play accented notes.",
])
story += [PageBreak()]

# 10 recording
story += [P("10  Recording patterns", "h1"),
          P("Recording always goes into the <b>selected slot</b> (the orange slot button). Press <b>REC</b>; what happens "
            "next depends on whether the pattern is playing."),
          P("Step record (REC on, stopped)", "h2"),
          P("A yellow cursor appears on step 1, and the status line reads "
            f"{M('STEP REC // STEP 1')}. Each note you play fills the step under the cursor and moves it on:")]
story += bullets([
    "<b>Notes</b> from your MIDI keyboard or the on-screen keys. Notes outside the C-to-C octave get OCT UP or OCT DN "
    "automatically.",
    "<b>Accent:</b> play harder (velocity 100+), or switch on <b>ACC</b> for the on-screen keys.",
    "<b>Slide:</b> hold the first note while you play the next (legato). On the on-screen keys, drag from one key "
    "to the next.",
    "<b>Rest:</b> press <b>REST</b>.",
    "Click a step number in the STEP row to move the cursor. At the end of the pattern it wraps back to step 1.",
])
story += [P("Live record (REC on, PLAY on)", "h2"),
          P("Play along with the running pattern. Each note lands on the <b>nearest step</b>, replacing what was there, "
            "and you hear it straight away. Accents and slides work the same way as in step record. Notes you don't "
            "replace stay as they were, so you can build a line up pass by pass. Use CLEAR first to start from nothing."),
          P("Fixing it up", "h2"),
          P("Everything recorded can be edited on the grid afterwards: drag notes, toggle modifiers, shift the pattern. "
            "When you like it, save it to the library from the pattern LCD menu."),
          Spacer(1, 6),
          note("Live recording works with the PLAY button. When a pattern is being played from your DAW with a "
               "trigger key (section 11), other keys transpose it instead of recording.", ORANGE),
          PageBreak()]

# 11 DAW
first, last = trig_name(24), trig_name(39)
story += [P("11  Using it in a DAW", "h1"),
          P("A sequencer step holds more than a MIDI note can - slides, accents, octave flags - so instead of turning "
            "patterns into MIDI, Bassline YF-21 lets your DAW <b>play its patterns</b>. <b>PLAY MODE</b> chooses how "
            "incoming MIDI notes are used."),
          table([["PLAY MODE", "TRIGGER KEYS", "OTHER KEYS"],
                 ["<b>PATTERN</b>", f"Hold a key ({first} to {last} by default) to play slot 1-16. Release it to stop.",
                  "While a slot is held: <b>transpose</b> the pattern. With no slot held: play the synth."],
                 ["<b>FREE</b>", "Play the synth like any other key.", "Play the synth."]],
                [1.0 * inch, 2.75 * inch, 2.75 * inch], accent=ORANGE)]
story += [P("PATTERN mode: playing slots from the piano roll", "h2")]
story += bullets([
    f"Draw a long <b>{first}</b> note: slot 1 plays for as long as the note lasts. {trig_name(25)} plays slot 2, and so "
    f"on up to {last} for slot 16. Change notes to change patterns - per bar, per phrase, wherever you like.",
    "The grid follows: whenever a trigger key plays a slot, the sequencer shows that slot, so you always see (and "
    "can edit) the pattern that's playing. Slots 9-16 start empty - fill them before triggering them.",
    "Nothing needs saving for this: all 16 slots are stored with your DAW project. Saving to the pattern library is "
    "only for using a pattern in other projects.",
    "<b>Transpose:</b> while a slot note is held, add a higher note. Its distance from <b>C3</b> (middle C) shifts the "
    "pattern: E3 = up 4 semitones, A2 = down 3, C4 = up an octave. When the transpose note ends, the pattern returns "
    "to its written key.",
    "The status line shows what's happening, e.g. " + M("PLAYING SLOT 2 (MIDI KEY)  TRANSPOSE +5") + ".",
    "Each note's end undoes exactly what its start did: a note that began as a synth note always releases the synth, "
    "even if a slot key started in the meantime. When the DAW stops, any slot started by a trigger key stops too.",
])
story += [P("Trigger keys", "h3"),
          P("<b>TRIGGER KEYS FROM</b> sets slot 1's key: any note from C-2 to C3, one semitone at a time. Slots 2-16 "
            "are the 15 keys above it. Some examples:"),
          table([["TRIGGER KEYS FROM", "SLOT 1", "SLOT 16", "WHEN TO USE IT"],
                 ["<b>C0</b> (default)", "C0 (24)", "D#1 (39)", "Easy to find in most piano rolls."],
                 ["<b>C-1</b>", "C-1 (12)", "D#0 (27)", "Frees C1-D#1 for playing low bass notes in PATTERN mode."],
                 ["<b>C-2</b>", "C-2 (0)", "D#-1 (15)", "The lowest notes MIDI has (note 0 is the bottom) - fully out of the way."],
                 ["<b>F-1</b>", "F-1 (17)", "G#0 (32)", "Any start note works, not just Cs."]],
                [1.35 * inch, 0.85 * inch, 0.85 * inch, 3.45 * inch]),
          P("Note names use middle C = C3 (MIDI 60), like Ableton, Akai and FL Studio. If your DAW calls middle C \"C4\", "
            "its names are one octave higher - the note numbers are what count.", "cap")]
story += [P("Pattern start: SYNC or RETRIG", "h3")]
story += bullets([
    "<b>SYNC</b> (default) - the pattern is locked to the bar: it plays whichever step matches the DAW's position. "
    "Start the note late and you join the pattern in the middle, always in time. Switching slots keeps the position.",
    "<b>RETRIG</b> - the pattern starts from step 1 on the next 16th note after the key goes down.",
])
story += [P("FREE mode: a normal synth", "h2"),
          P("Every key plays the synth and nothing triggers patterns - record and edit basslines in the piano roll like "
            "any other instrument. The classic behaviour still works from MIDI:"),
          ]
story += bullets([
    "<b>Overlapping notes slide</b>: let a note run into the start of the next one and it glides there without "
    "retriggering. Releasing the newer note while the older one is still held glides back.",
    "<b>Velocity 100 or more is an accent.</b>",
])
story += [Spacer(1, 4),
          note("PATTERN mode with no slot held behaves the same way for keys outside the trigger range, so you can mix "
               "pattern sections and hand-played sections on one track. PLAY MODE is saved with the project and can be "
               "automated or MIDI-learned.", CYAN),
          PageBreak()]

# 12 tempo
story += [P("12  Tempo and sync", "h1")]
story += bullets([
    "<b>SYNC on</b> (default) - the plugin uses the DAW's tempo (the display shows <b>HOST</b>). When the DAW is "
    "playing, patterns and the synced LFO line up with the DAW's bars and beats.",
    "<b>SYNC off</b>, or in the Standalone - the plugin uses its own tempo (<b>INT</b>), set by dragging or scrolling "
    "the tempo display (40 to 240 BPM).",
    "The <b>PLAY</b> button follows the DAW's position too when SYNC is on and the DAW is playing; with the DAW "
    "stopped it simply starts from step 1 on its own clock.",
    "Trigger keys played while the DAW is stopped (for example when jamming on a keyboard) start the slot on the "
    "plugin's own clock.",
])
story += [Spacer(1, 6),
          note("Patterns assume 4/4: step 1 falls on each bar line, counted from the start of the DAW's timeline.", ORANGE)]

# 13 MIDI learn
story += [P("13  MIDI learn", "h1")]
story += bullets([
    "Click <b>MIDI LEARN</b> in the header. Every control gets a yellow outline with its controller number "
    f"({M('CC 74')}) or {M('--')} if it has none.",
    "Click a control, then move a knob or fader on your controller. Done - click the next one to keep going.",
    "<b>Esc</b> cancels; click MIDI LEARN again when you're finished. Right-click a control in learn mode to clear it.",
    "Outside learn mode, right-click any knob, button or menu and choose <b>Learn MIDI CC...</b>",
    "Each control listens to one controller; giving a controller to a new control frees the old one.",
    "Buttons switch on above the middle of the controller's travel; menus step through their choices.",
])
story += [shot("learn_mode.png", width=5.6 * inch, caption="MIDI LEARN mode."),
          note("Controller assignments are saved with your <b>project</b>, not with patches - they describe your "
               "controller, so they stay put when you change sounds.", ORANGE), PageBreak()]

# 14 patches and patterns
patch_desc = {
    "INIT": "Every control at its default: one saw through the 303 filter.",
    "CLASSIC ACID": "The textbook acid squelch - darker cutoff, more resonance and accent.",
    "SQUARE SQUELCH": "Square wave, high resonance and filter drive: hollow and rubbery.",
    "YF-21 SCREAMER": "Near-whistling resonance, long decay and soft distortion. Ride the cutoff.",
    "GHOST DRONE": "Three detuned oscillators, ladder filter, sustain and a slow LFO on cutoff.",
    "SUPERNOVA STACK": "Saw plus a fifth plus a sub square: a big, wide acid chord-bass.",
    "FOLD SPACE": "Triangle through the SVF into the wave folder: metallic and alive.",
    "DIGI FORMANT": "Talking formant wavetable scanned by a 1-bar synced LFO.",
    "PHASE BITE": "Phase-distortion wavetable with LFO movement and tube drive.",
    "BRAIN CRUSH": "Bit-crushed wavetable into the CRUSH distortion: lo-fi and digital.",
    "RUBBER SUB": "Shaped sine plus a quiet triangle an octave up, longer slides: round sub bass.",
    "GLASS WIRE": "FM glass and sync wavetables with the SVF: bright, plucky, high register.",
    "EDEN WOBBLE": "Synced 1/8T LFO on pulse width and cutoff with sustain: a wobble bass.",
}
patch_names = re.findall(r'\{\s*"([A-Z0-9 \-]+)",\s*\{', open(os.path.join(ROOT, "Source", "FactoryPatches.h")).read())
pattern_defs = re.findall(r'\{\s*"([A-Z0-9 \-]+)",\s*(\d+),\s*"([^"]*)",\s*([0-9.]+)f',
                          open(os.path.join(ROOT, "Source", "FactoryPatterns.h")).read())
story += [P("14  Patches and patterns", "h1"),
          P("Sounds and notes are stored separately, so any pattern can be played with any sound."),
          table([["", "PATCH", "PATTERN"],
                 ["<b>Holds</b>", "Every sound control: oscillators, LFO, filter, envelopes, effects, master.",
                  "16 steps (notes and modifiers), length, swing and gate."],
                 ["<b>Not included</b>", "Tempo, sync, swing, gate, play mode, pattern start, trigger keys, MIDI CCs.",
                  "The sound."],
                 ["<b>Where</b>", "The patch LCD in the header.", "The pattern LCD in the sequencer (into the selected slot)."],
                 ["<b>File</b>", M(".yfpatch"), M(".yfpattern")]],
                [1.1 * inch, 2.7 * inch, 2.7 * inch]),
          P("Using them", "h2")]
story += bullets([
    "<b>Load:</b> click the LCD for the list (factory first, then yours). The header's arrows step through patches.",
    "<b>Save:</b> click the LCD - <b>Save As...</b> names a new one, <b>Save</b> updates the user patch or pattern you "
    "loaded. <b>Delete</b> removes it from the library (what's loaded stays).",
    "Loading a pattern puts it into the selected slot and sets SWING and GATE to the values it was saved with.",
    "Your project remembers the loaded patch (with unsaved changes), all 16 slots and their names.",
    "Choose patches here rather than from your DAW's preset menu. DAW presets still work - they store everything.",
])
story += [P("Where they are stored", "h3"),
          table([["SYSTEM", "FOLDER (Patches and Patterns inside it)"],
                 ["Windows", "%APPDATA%&#92;Bassline YF-21&#92;   (paste into the Explorer address bar)"],
                 ["macOS", "~/Library/Bassline YF-21/"],
                 ["Linux", "~/.config/Bassline YF-21/"]], [1.2 * inch, 5.3 * inch]),
          P("The files are small XML: copy them between computers or share them. The plugin and the Standalone share them.", "cap"),
          CondPageBreak(3.5 * inch),
          P("Factory patches", "h2"),
          table([["PATCH", "CHARACTER"]] + [[f"<b>{n}</b>", patch_desc.get(n, "")] for n in patch_names],
                [1.6 * inch, 4.9 * inch]),
          CondPageBreak(2.5 * inch),
          P("Factory patterns", "h2"),
          P("A new instance starts with factory patterns 2-9 in slots 1-8 (in this order); slots 9-16 start empty."),
          table([["PATTERN", "LENGTH", "SWING", "NOTES"]] +
                [[f"<b>{n}</b>", ln, f"{round(50 + float(sw) * 25)}%", M(st.replace("~", "&#126;").replace(">", "&gt;"))]
                 for n, ln, st, sw in pattern_defs],
                [1.4 * inch, 0.6 * inch, 0.6 * inch, 3.9 * inch]),
          P(f"Pattern notes: C' = top C, {M('^')} octave up, {M('v')} octave down, {M('&gt;')} accent, {M('&#126;')} slide, "
            f"{M('-')} rest.", "cap"),
          PageBreak()]

# 15 tips
tips = [
    ("The classic squelch", "303 DIODE, RESO 70-85%, ENV MOD 50-70%, DECAY 200-400 ms. Accents on off-beats, a slide "
                            "every few steps. Slowly open CUTOFF over 16 bars."),
    ("Climbing accents", "Put accents on 3-4 steps in a row and turn ACCENT up: each one screams higher than the last."),
    ("Rubbery, not harsh", "SQUARE wave, 24DB LADDER, less RESO, longer GATE and more slides."),
    ("Detuned stack", "OSC 2 on, same wave, FINE +7 to +12 ct, LEVEL a little under OSC 1."),
    ("Fifth / octave bass", "OSC 2 SEMI +7 (fifth) or OCT +1; OSC 3 SINE OCT -1 for weight."),
    ("Talking bass", "WT FORMANT, its LFO button on, LFO SHAPE 70%+, synced at 1/2 or 1 BAR."),
    ("PWM movement", "SQUARE, LFO button on, LFO SHAPE 40%, slow rate."),
    ("Metallic acid", "WAVE FOLDER FOLD 30-60%, a little SYM. Lower the filter RESO so it doesn't get piercing."),
    ("Dirt without mud", "DISTORTION after a bright filter: TONE 50-60%, MIX 60-80% keeps the low end clean."),
    ("Odd-length lines", "LEN 7, 11 or 13: the pattern drifts against the bar and keeps changing."),
    ("Key changes from the DAW", "PATTERN mode: hold the slot note for the whole section, add F3 or G3 notes "
                                 "for the bars that move up."),
]
story += [P("15  Sound design tips", "h1"),
          table([["IDEA", "TRY THIS"]] + [[f"<b>{a}</b>", b] for a, b in tips], [1.6 * inch, 4.9 * inch]),
          Spacer(1, 8),
          note("<b>Keep a knob under your hand:</b> MIDI-learn CUTOFF and ENV MOD to two controller knobs and ride them "
               "while the pattern plays - that's how acid is performed.", ORANGE)]

# 16 troubleshooting
trouble = [
    ("A note hangs / drones", "Press <b>PANIC</b> in the header. (MIDI \"all notes off\" from your DAW or controller does the same.)"),
    ("Pressing a low key starts a pattern instead of a note",
     "PATTERN mode: that key is a trigger key. Switch PLAY MODE to FREE, or move TRIGGER KEYS FROM lower (e.g. C-2)."),
    ("Keys don't make sound while a pattern plays from the DAW",
     "In PATTERN mode, keys transpose the pattern while a slot key is held. Use FREE mode or a second instance to play on top."),
    ("Pattern plays out of time / starts in the middle",
     "That's PATTERN START = SYNC (locked to the bar). Choose RETRIG to always start from step 1."),
    ("The pattern doesn't follow the DAW tempo", "Switch SYNC on (header)."),
    ("Nothing happens when I press PLAY in a DAW",
     "Check the track is armed / monitoring so you can hear the instrument, and that MASTER VOLUME isn't down."),
    ("A trigger key plays nothing", "That slot is empty (slots 9-16 start empty). The grid shows the slot when its key "
     "is played - fill it, or load a pattern into it from the pattern LCD."),
    ("Recording doesn't change anything",
     "Recording goes into the selected (orange) slot. In live record, notes go to the nearest step of that slot."),
    ("No sound from the Standalone",
     "Options > Audio/MIDI Settings: pick an output device. On Linux, try another ALSA device, or JACK / pw-jack."),
    ("Harsh or very loud with high resonance",
     "Lower RESO or FILTER DRIVE, or try the 303 DIODE mode, which thins out as resonance rises."),
    ("DAW doesn't list the plugin", "Check the .vst3 folder is in your VST3 folder (section 2) and rescan plugins."),
    ("Windows says the app is unsafe", "Click More info > Run anyway. The app isn't signed with a paid certificate."),
]
story += [PageBreak(), P("16  Troubleshooting", "h1"),
          table([["PROBLEM", "WHAT TO DO"]] + [[f"<b>{a}</b>", b] for a, b in trouble], [2.3 * inch, 4.2 * inch]),
          PageBreak()]

# 17 credits
story += [P("17  Credits and license", "h1"),
          P("Bassline YF-21 is free software: you can redistribute it and/or modify it under the terms of the "
            "<b>GNU Affero General Public License</b>, version 3 or (at your option) any later version. It is "
            "distributed in the hope that it will be useful, but <b>without any warranty</b>; see the LICENSE file."),
          P("In short: you can use, study, change and share it. If you distribute a modified version, you must make its "
            "source code available under the same license."),
          P("Source code: " + M("https://github.com/LexLongcoding/BasslineYF21")),
          P("Built with", "h2")]
story += bullets([
    "<b>JUCE 8</b> (juce.com), used under the AGPLv3.",
    "<b>VST 3 SDK</b> by Steinberg Media Technologies GmbH. VST is a trademark of Steinberg Media Technologies GmbH; "
    "Bassline YF-21 is not affiliated with Steinberg.",
    "<b>Orbitron</b> by The League of Moveable Type (instanced and renamed \"Decu Display\") and <b>Share Tech Mono</b> "
    "by Carrois Type Design - both under the SIL Open Font License 1.1.",
    "Inspired by the Roland TB-303 and the Korg Volca Bass. Not affiliated with Roland or Korg.",
])
story += [Spacer(1, 10), P(f"Bassline YF-21 - Copyright (C) 2026 LexLongcoding. Manual for version {VERSION}.", "cap")]

# ---------------------------------------------------------------- build
doc = BaseDocTemplate(OUT, pagesize=letter, leftMargin=inch, rightMargin=inch, topMargin=0.85 * inch,
                      bottomMargin=0.8 * inch, title="Bassline YF-21 User Manual", author="LexLongcoding")
frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
doc.addPageTemplates([PageTemplate(id="cover", frames=[frame], onPage=on_cover),
                      PageTemplate(id="body", frames=[frame], onPage=on_page)])
doc.build(story)
print("wrote", OUT)
