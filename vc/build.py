"""Build the Replay Editor CLEO mod for GTA Vice City v1.0.

CLEO script: INI options + loads the native module.
  * LOOKAROUND_AUTO_RETURN=0 : replay look-around camera no longer returns to
    the recorded camera 60 frames after the mouse stops (NOP 'dec [0x94DD48]').
  * LOOKAROUND=0             : disable the mouse look-around camera entirely.

Native module native/replayeditor.c -> CLEO/ReplayEditor/ReplayEditor.dll,
loaded by the script (0AA2/0AA4/0AA7 RE_Init): F6 menu (open in editor /
play replay, file list of the user folder), editor, F7 save.

Outputs: ../CLEO/ReplayEditor.cs, ../CLEO/ReplayEditor.ini (kept if present),
         ../CLEO/ReplayEditor/ReplayEditor.dll
"""
import os
import shutil
import subprocess
import sys

from scm import Scm, L, IF

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_CLEO = os.path.join(HERE, "..", "CLEO")

# --- gta-vc.exe v1.0 patch sites (verified with REA/Ghidra) ----------------
LOOK_DEC = 0x61EA8B          # FF 0D 48 DD 94 00   dec dword [0x94DD48]
LOOK_DEC_ORIG = (0xDD480DFF, 0x0094)
LOOK_JZ = 0x61E97F           # 0F 84 D3 05 00 00   jz end  (if !bAllowLookAroundCam)
LOOK_JZ_ORIG, LOOK_JZ_JMP = 0x840F, 0xE990     # -> 90 E9 (same rel32)
LOAD_NAME_OPND = 0x61F0EE    # PlayReplayFromHD: 68 <04 B6 6D 00> push "replay.rep"
REPLAY_REP = 0x6DB604
DLL_REL = "CLEO\\ReplayEditor\\ReplayEditor.dll"
MSVC = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\bin\Hostx86\x86"

INI_TEXT = """\
; Replay Editor for GTA Vice City - settings (read when the script starts)
[Playback]
; 0 = once you move the mouse during a replay, the look-around camera stays
;     where you left it (default).
; 1 = original game behaviour: about 2 seconds (60 frames) after the mouse
;     stops, the camera jumps back to the recorded camera.
LOOKAROUND_AUTO_RETURN=0
; 1 = mouse look-around camera during replays (default), 0 = disabled: the
;     replay always shows the recorded (or edited) camera.
LOOKAROUND=1
"""
INI_DISPLAY = """\
[Display]
; size of all Replay Editor texts (menu, help, status, messages) in percent:
; 100 = original size, 90, 80, 70, 60 (default) ... (allowed 40 - 150)
FONT_SCALE=60
"""
INI_KEYS = """\
[Keys]
; keys of the Replay Editor, each one of F4, F5, F6, F7, F8, F9, F10, F11, F12.
; Every key must be different: if the same key is set twice, the menu key only
; shows an error message. An invalid value uses the default.
; MENU    = open / close the menu, exit the editor (default F6)
; SAVE    = save the project being edited (default F7)
; SAVE_AS = save as a new project file (default F8)
MENU=F6
SAVE=F7
SAVE_AS=F8
"""


def build_script():
    s = Scm()
    i, lv, st, t8 = s.i, s.lv, s.s, s.t8
    INI = "CLEO\\ReplayEditor.ini"
    # locals: 1@ auto return, 4@ lookaround, 5@ dll, 6@ RE_Init, 7@ result
    s.op(0x0000)
    s.op(0x03A4, t8("REPEDIT"))
    s.op(0x0006, lv(1), i(0))
    s.op(0x0006, lv(4), i(1))
    s.op(0x0AF0, st(INI), st("Playback"), st("LOOKAROUND_AUTO_RETURN"), lv(1))
    s.op(0x0AF0, st(INI), st("Playback"), st("LOOKAROUND"), lv(4))
    s.op(0x0A8C, i(LOAD_NAME_OPND), i(4), i(REPLAY_REP), i(1))     # clean state

    # look-around auto return
    s.op(0x00D6, IF)
    s.op(0x0039, lv(1), i(0))
    s.op(0x004D, L("keep_return"))
    s.op(0x0A8C, i(LOOK_DEC), i(4), i(0x90909090), i(1))
    s.op(0x0A8C, i(LOOK_DEC + 4), i(2), i(0x9090), i(1))
    s.op(0x0002, L("look_on"))
    s.label("keep_return")
    s.op(0x0A8C, i(LOOK_DEC), i(4), i(LOOK_DEC_ORIG[0]), i(1))
    s.op(0x0A8C, i(LOOK_DEC + 4), i(2), i(LOOK_DEC_ORIG[1]), i(1))
    # look-around on/off
    s.label("look_on")
    s.op(0x00D6, IF)
    s.op(0x0039, lv(4), i(0))
    s.op(0x004D, L("look_enabled"))
    s.op(0x0A8C, i(LOOK_JZ), i(2), i(LOOK_JZ_JMP), i(1))
    s.op(0x0002, L("load_dll"))
    s.label("look_enabled")
    s.op(0x0A8C, i(LOOK_JZ), i(2), i(LOOK_JZ_ORIG), i(1))

    # native editor module
    s.label("load_dll")
    s.op(0x00D6, IF)
    s.op(0x0AA2, st(DLL_REL), lv(5))
    s.op(0x004D, L("loop"))
    s.op(0x00D6, IF)
    s.op(0x0AA4, st("RE_Init"), lv(5), lv(6))
    s.op(0x004D, L("loop"))
    # 0AA5/0AA7 are variadic in III.VC.CLEO: after the arguments (and the
    # return variable) the handler skips params until a 0x00 type byte and
    # then steps over it, so the argument list must end with a 0 terminator.
    s.op(0x0AA7, lv(6), i(0), i(0), lv(7))
    s.raw(b"\x00")

    # keep the script alive (the DLL does all the work from its hooks)
    s.label("loop")
    s.op(0x0001, i(1000))
    s.op(0x0002, L("loop"))
    return s.build()


def build_native():
    src = os.path.join(HERE, "native", "replayeditor.c")
    obj_dir = os.path.join(HERE, "native", "obj")
    out_dir = os.path.join(OUT_CLEO, "ReplayEditor")
    os.makedirs(obj_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)
    obj = os.path.join(obj_dir, "replayeditor.obj")
    built = os.path.join(obj_dir, "ReplayEditor.dll")
    dll = os.path.join(out_dir, "ReplayEditor.dll")
    run = lambda args: subprocess.run(args, check=True, cwd=obj_dir)
    run([os.path.join(MSVC, "cl.exe"), "/nologo", "/c", "/O1", "/GS-", "/Zl", "/Oi-", "/W3",
         "/Gs1000000", "/fp:precise", "/Fo" + obj, src])
    run([os.path.join(MSVC, "link.exe"), "/nologo", "/DLL", "/NODEFAULTLIB", "/SAFESEH:NO",
         "/ENTRY:_DllMainCRTStartup", "/MACHINE:X86", "/OUT:" + built, obj])
    print("ReplayEditor.dll %d bytes" % os.path.getsize(built))
    try:
        shutil.copyfile(built, dll)
    except PermissionError:
        print("WARNING: %s is in use (game running?) - close the game and run build.py again" % dll)


def main():
    build_native()
    for k in range(1, 10):                                  # SUB 1 .. SUB 9 looks folders
        os.makedirs(os.path.join(OUT_CLEO, "ReplayEditor", "Looks", "SUB%d" % k), exist_ok=True)
    data = build_script()
    with open(os.path.join(OUT_CLEO, "ReplayEditor.cs"), "wb") as fh:
        fh.write(data)
    ini = os.path.join(OUT_CLEO, "ReplayEditor.ini")
    if not os.path.exists(ini) or "--reset-ini" in sys.argv:
        with open(ini, "w", newline="\r\n") as fh:
            fh.write(INI_TEXT + "\n" + INI_DISPLAY + "\n" + INI_KEYS)
    else:                                                               # older INI: add new sections
        old = open(ini, encoding="latin-1").read()
        for name, sec in (("[Display]", INI_DISPLAY), ("[Keys]", INI_KEYS)):
            if name not in old:
                with open(ini, "a", newline="\r\n") as fh:
                    fh.write("\n" + sec)
    print("ReplayEditor.cs %d bytes" % len(data))


if __name__ == "__main__":
    main()
