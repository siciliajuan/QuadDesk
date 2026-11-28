# QuadDesk — user interface texts (English / Spanish)

Source for the language DLLs `QDESKEN.DLL` and `QDESKES.DLL` (D-126,
D-127). Every text the user can see is here. `&` marks the access key
(underlined letter); `%d` and `%s` are filled in by the program.

Status: **proposal for review** (milestone 4, part C). Items marked ❓
need a decision.

Spanish terms follow the Spanish edition of Windows 3.1: *Aceptar*,
*Cancelar*, *Mayús*, *Supr*, *Re Pág*, *Av Pág*, *Panel de control*,
*Administrador de programas*.

## 1. Pager: menus and system menu

| ID | English | Spanish | Notes |
|---|---|---|---|
| M1 | `&Options` | `&Opciones` | |
| M2 | `&Preferences...` | `&Preferencias...` | Also in the pager's system menu |
| M3 | `&Help` | `Ay&uda` | Spanish Windows 3.1 uses *Ay&uda* |
| M4 | `&Contents` | `&Contenido` | Greyed until v0.6 |
| M5 | `&Search for Help On...` | `&Buscar ayuda sobre...` | Greyed until v0.6 |
| M6 | `&How to Use Help` | `C&ómo usar la Ayuda` | Greyed until v0.6 |
| M7 | `&About QuadDesk...` | `&Acerca de QuadDesk...` | Also in the pager's system menu |

## 2. Pager window

| ID | English | Spanish | Notes |
|---|---|---|---|
| P1 | `Desktop %d` | `Escritorio %d` | Cell label |
| P2 | `Windows on desktop %d (%d):` | `Ventanas del escritorio %d (%d):` | |
| P3 | `Minimized applications (%d):` | `Aplicaciones minimizadas (%d):` | |
| P4 | `Arrows` | `flechas` | Status bar: `Ctrl+Alt+Shift+Arrows` / `Ctrl+Alt+Mayús+flechas` |

## 3. "Move to Desktop" in every window's system menu

| ID | English | Spanish | Notes |
|---|---|---|---|
| S1 | `Move to &Desktop` | `Mover al &escritorio` | In Spanish, *M* is taken by *Mover* in the system menu |
| S2 | `Desktop &%d` | `Escritorio &%d` | 1 to 4 |

## 4. Preferences dialog (QUADDESK.EXE and QDESK.CPL)

| ID | English | Spanish | Notes |
|---|---|---|---|
| D1 | `QuadDesk Preferences` | `Preferencias de QuadDesk` | Caption |
| D2 | `Shortcuts` | `Teclas de acceso rápido` | Group (as in the canvas mock-up) |
| D3 | `&Up:` | `&Arriba:` | |
| D4 | `&Down:` | `A&bajo:` | |
| D5 | `&Left:` | `I&zquierda:` | *I* is for *Iniciar* (D9) |
| D6 | `&Right:` | `&Derecha:` | |
| D7 | `Sho&w/hide:` | `&Mostrar/ocultar:` | |
| D8 | `Ctrl` · `Alt` · `Shift` | `Ctrl` · `Alt` · `Mayús` | Check boxes |
| D9 | `When minimized` | `Al minimizar` | Group |
| D10 | `Show &icon` | `Mostrar el ic&ono` | |
| D11 | `Start-up` | `Inicio` | Group |
| D12 | `&Start QuadDesk with Windows` | `&Iniciar QuadDesk al arrancar Windows` | |
| D13 | `OK` · `Cancel` | `Aceptar` · `Cancelar` | |

### Key names in the drop-down lists

| English | Spanish |
|---|---|
| `Up Arrow` · `Down Arrow` · `Left Arrow` · `Right Arrow` | `Flecha arriba` · `Flecha abajo` · `Flecha izquierda` · `Flecha derecha` |
| `Space` | `Espacio` |
| `Home` · `End` | `Inicio` · `Fin` |
| `Page Up` · `Page Down` | `Re Pág` · `Av Pág` |
| `Insert` · `Delete` | `Insert` · `Supr` |
| `A`–`Z`, `0`–`9`, `F1`–`F12` | the same |

In messages and the status bar, shortcuts are written `Ctrl+Alt+Shift+Space`
/ `Ctrl+Alt+Mayús+Espacio`. QUADDESK.INI always keeps the English names.

### Names of the shortcuts in messages

| English | Spanish |
|---|---|
| `Up` · `Down` · `Left` · `Right` | `Arriba` · `Abajo` · `Izquierda` · `Derecha` |
| `Show/hide QuadDesk` | `Mostrar/ocultar QuadDesk` |

## 5. Messages

| ID | English | Spanish | Notes |
|---|---|---|---|
| E1 | `The shortcut for "%s" must include at least one of Ctrl or Alt.` | `El atajo de «%s» debe incluir al menos una de estas teclas: Ctrl o Alt.` | Wording by Juan; the rule is kept (D-135) |
| E2 | `%s is reserved by Windows.`<br><br>`Choose another shortcut for "%s".` | `Windows reserva %s.`<br><br>`Elija otro atajo para «%s».` | |
| E3 | `%s is used for "%s" and for "%s".`<br><br>`Each shortcut must be different.` | `%s está asignado a «%s» y a «%s».`<br><br>`Cada atajo debe ser distinto.` | |
| E4 | `Without an icon, QuadDesk can only be shown with the shortcut %s.`<br><br>`While it is hidden, the preferences are only available in the Control Panel.` | `Sin icono, QuadDesk solo se podrá mostrar con el atajo %s.`<br><br>`Mientras esté oculto, las preferencias solo estarán disponibles en el Panel de control.` | As in the canvas mock-up |
| E5 | `QuadDesk is already running.` | `QuadDesk ya está en marcha.` | ❓ Disappears if a second launch shows the running QuadDesk (proposal pending) |
| E6 | `QuadDesk could not start: it could not install its keyboard and window hooks (QDHOOK.DLL).` | `QuadDesk no ha podido arrancar: no ha podido instalar sus enlaces con el teclado y las ventanas (QDHOOK.DLL).` | Today: "could not install its system hooks" |
| E7 | `QuadDesk cannot find its language files (QDESKEN.DLL, QDESKES.DLL). Please reinstall QuadDesk.` | — | Fixed text in QUADDESK.EXE, English only (D-127) |

Spanish messages use *usted* (*Elija*), as Windows 3.1 does.

## 6. Control Panel and About

| ID | English | Spanish | Notes |
|---|---|---|---|
| C1 | `QuadDesk` | `QuadDesk` | Name under the icon |
| C2 | `Sets the shortcuts and options of QuadDesk virtual desktops.` | `Atajos y opciones de los escritorios virtuales de QuadDesk.` | At most 63 characters |
| A1 | `About QuadDesk` | `Acerca de QuadDesk` | Caption |
| A2 | `Virtual desktops for Windows 3.1` | `Escritorios virtuales para Windows 3.1` | |
| A3 | `Copyright © 2026 Juan Sicilia` | `Copyright © 2026 Juan Sicilia` | |
| A4 | `Released under the MIT License.` | `Publicado con la licencia MIT.` | The About box is reviewed in v0.6 (D-069) |

## 7. Status bar (D-129, D-136)

`Ctrl+Alt+Shift+Arrows` / `Ctrl+Alt+Mayús+flechas` (with the real
modifiers) when the four arrow shortcuts share their modifiers; otherwise
only the show/hide shortcut, with no label, e.g. `Ctrl+Alt+Shift+Space` /
`Ctrl+Alt+Mayús+Espacio`. Both fit in its 170 px.
