#!/usr/bin/env python3
"""Analisis del experimento del motor (puntos 1 a 5 de la actividad).

Uso (desde la carpeta Experimento_PWM):
    python analizar_motor.py datos_Daniel.log --nombre Daniel
    python analizar_motor.py datos_Daniel.log datos_Daniel_escalon.log --nombre Daniel
        (con varios logs se usa la ULTIMA prueba de cada tipo, en el orden en que los pongas)

Genera en la carpeta resultados_<Nombre>/:
    1_escalon_datos_vs_modelo.png   PWM y RPM medidas del escalon + modelo 1
    2_prbs9_datos_vs_modelo.png     PWM y RPM medidas de la PRBS9 (primeros 6 s) + modelo 2
    3_error_escalon.png             error del modelo 1 y su fit
    4_error_prbs9.png               error del modelo 2 y su fit
    resultados.txt                  funciones de transferencia, fit y error
    Reporte_<Nombre>.docx           borrador del reporte (agrega --sin-reporte para no crearlo)

Modelo 1 (respuesta al paso):  G(s) = K / (tau s + 1)
    K   = cambio de RPM / cambio de PWM
    tau = tiempo en llegar al 63.2 % del cambio
Modelo 2 (minimos cuadrados con PRBS9):  y(k) = a y(k-1) + b u(k-1)
    theta = [a b] = (Phi^T Phi)^-1 Phi^T Y,   K = b/(1-a),   tau = -Ts/ln(a)
Fit: 100 (1 - ||y - y_modelo|| / ||y - promedio(y)||)    (100 % = perfecto)
"""
import argparse
import json
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

GRIS = "#52514e"       # datos medidos
AZUL = "#2a78d6"       # modelo 1 (escalon)
NARANJA = "#eb6834"    # modelo 2 (PRBS9)
INTEGRANTES = {1: "Daniel", 2: "Mateo", 3: "Angel"}
VENTANA_PRBS_S = 6.0   # segundos de la PRBS9 que se dibujan (se ve mejor que los 30 s)


# =========================================================================== 1. leer el log
def leer_log(ruta):
    """Lista de pruebas completas. Cada una: {meta, pwm (array), cuenta (array)}."""
    pruebas, meta, pwm, cuenta = [], None, [], []
    with open(ruta, encoding="utf-8", errors="replace") as f:
        for linea in f:
            partes = linea.strip().split(",")
            if len(partes) < 2 or partes[0] != "EXP":
                continue
            if partes[1] == "INICIO":                       # encabezado: run=1,modo=1,...
                meta = {}
                for p in partes[2:]:
                    if "=" in p:
                        clave, valor = p.split("=", 1)
                        meta[clave] = int(valor)
                pwm, cuenta = [], []
            elif partes[1] == "FIN":                        # prueba completa
                if meta is not None and len(pwm) == meta.get("n", -1):
                    pruebas.append({"meta": meta, "pwm": np.array(pwm, float),
                                    "cuenta": np.array(cuenta, np.int64)})
                meta = None
            elif partes[1].isdigit() and meta is not None and len(partes) == 4:
                pwm.append(int(partes[2]))                  # EXP,<k>,<pwm>,<cuenta>
                cuenta.append(int(partes[3]))
    return pruebas


# =========================================================================== 2. cuentas -> RPM
def senales(prueba):
    """Devuelve t (s), u (PWM aplicado en cada intervalo, %), y (RPM medidas) y Ts (s)."""
    meta = prueba["meta"]
    ts = meta.get("ts_ms", 10) / 1000.0
    d = np.diff(prueba["cuenta"])                           # cuentas que avanzo en cada 10 ms
    d = ((d + 32768) % 65536) - 32768                       # el contador de 16 bits "da la vuelta"
    y = meta.get("sgn", -1) * d / meta.get("cpv", 685) * 60.0 / ts   # cuentas -> RPM
    u = prueba["pwm"][:-1]                                  # PWM que actuo en ese intervalo
    t = np.arange(1, len(prueba["cuenta"])) * ts
    return t, u, y, ts


# =========================================================================== 3. modelos
def simular(a, b, u, y_medida, u0, y0):
    """Simula y(k) = a y(k-1) + b u(k-1) en desviaciones respecto a (u0, y0),
    partiendo del primer valor medido. Solo usa el PWM; devuelve RPM."""
    du = u - u0
    yh = np.empty(len(u))
    yh[0] = y_medida[0] - y0
    for k in range(1, len(u)):
        yh[k] = a * yh[k - 1] + b * du[k]
    return yh + y0


def fit(y, yh):
    return 100.0 * (1.0 - np.linalg.norm(y - yh) / np.linalg.norm(y - y.mean()))


def modelo_escalon(prueba):
    t, u, y, ts = senales(prueba)
    t_salto = prueba["meta"]["ini"] * ts
    antes = (t <= t_salto + 1e-9) & (t > t_salto - 0.5)     # 0.5 s antes del salto
    final = t > t[-1] - 2.0                                 # ultimos 2 s
    u0, y0 = u[antes].mean(), y[antes].mean()
    u1, y1 = u[final].mean(), y[final].mean()
    K = (y1 - y0) / (u1 - u0)

    objetivo = y0 + 0.632 * (y1 - y0)                       # 63.2 % del cambio
    tau = None
    for k in np.where(t > t_salto + 1e-9)[0]:
        if y[k] >= objetivo:                                # primera muestra que lo pasa
            t_cruce = t[k - 1] + (objetivo - y[k - 1]) / (y[k] - y[k - 1]) * ts   # interpolacion
            tau = t_cruce - t_salto
            break
    if tau is None:
        sys.exit("La velocidad no llego al 63.2 % del cambio: revisa la prueba del escalon.")

    a = float(np.exp(-ts / tau))                            # discreto (ZOH)
    b = float(K * (1.0 - a))
    yh = simular(a, b, u, y, u0, y0)
    return {"K": float(K), "tau": float(tau), "a": a, "b": b, "ts": ts,
            "u0": float(u0), "y0": float(y0), "u1": float(u1), "y1": float(y1),
            "objetivo": float(objetivo), "t_salto": t_salto,
            "t": t, "u": u, "y": y, "yh": yh, "fit": fit(y, yh)}


def modelo_prbs(prueba):
    t, u, y, ts = senales(prueba)
    t_ini = prueba["meta"]["ini"] * ts
    dentro = (t - ts) >= t_ini - 1e-9                       # solo la secuencia (sin el calentamiento)
    t, u, y = t[dentro] - t_ini, u[dentro], y[dentro]
    u0, y0 = u.mean(), y.mean()                             # punto de operacion
    du, dy = u - u0, y - y0

    Phi = np.column_stack([dy[:-1], du[1:]])                # renglones: [y(k-1)  u(k-1)]
    Y = dy[1:]                                              #            y(k)
    a, b = np.linalg.solve(Phi.T @ Phi, Phi.T @ Y)          # theta = (Phi^T Phi)^-1 Phi^T Y
    a, b = float(a), float(b)
    K = b / (1.0 - a)
    tau = -ts / np.log(a) if 0.0 < a < 1.0 else float("nan")
    yh = simular(a, b, u, y, u0, y0)
    return {"K": float(K), "tau": float(tau), "a": a, "b": b, "ts": ts,
            "u0": float(u0), "y0": float(y0), "N": int(len(Y)),
            "t": t, "u": u, "y": y, "yh": yh, "fit": fit(y, yh)}


def tau_subida_bajada(t, u, y, minimo=18):
    """tau al 63.2 % en los bits largos de la PRBS9, separando subidas y bajadas de PWM."""
    cambios = np.where(np.diff(u) != 0)[0] + 1
    sube, baja = [], []
    for i in range(1, len(cambios) - 1):
        c = cambios[i]
        if c - cambios[i - 1] < minimo or cambios[i + 1] - c < minimo:
            continue
        y0, y1 = y[c - 3:c].mean(), y[cambios[i + 1] - 5:cambios[i + 1]].mean()
        objetivo, signo = y0 + 0.632 * (y1 - y0), np.sign(y1 - y0)
        for j in range(c, cambios[i + 1]):
            if (y[j] - objetivo) * signo >= 0:
                f = (objetivo - y[j - 1]) / (y[j] - y[j - 1]) if y[j] != y[j - 1] else 1.0
                (sube if y1 > y0 else baja).append(t[j - 1] + f * (t[j] - t[j - 1]) - t[c - 1])
                break
    return sube, baja


# =========================================================================== 4. graficas
def estilo(ax):
    for lado in ("top", "right"):
        ax.spines[lado].set_visible(False)
    ax.grid(True, color="#e8e7e2")
    ax.set_axisbelow(True)


def grafica_datos_vs_modelo(m, color, etiqueta, titulo, destino, xlim=None):
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(10, 6), sharex=True,
                                 gridspec_kw={"height_ratios": [1, 2]})
    a1.step(m["t"], m["u"], where="pre", color=GRIS, linewidth=2)
    a1.set_ylabel("PWM (%)")
    a1.set_title(titulo, loc="left")
    a2.plot(m["t"], m["y"], color=GRIS, linewidth=1.5, label="RPM medidas (experimental)")
    a2.plot(m["t"], m["yh"], color=color, linewidth=2, linestyle="--",
            label=f"{etiqueta} (fit {m['fit']:.1f} %)")
    a2.set_ylabel("Velocidad (RPM)")
    a2.set_xlabel("Tiempo (s)")
    a2.legend(loc="lower left", bbox_to_anchor=(0.0, 1.0), ncol=2, frameon=False)
    for a in (a1, a2):
        estilo(a)
    if xlim:
        a2.set_xlim(*xlim)
    fig.tight_layout()
    fig.savefig(destino, dpi=150)
    plt.close(fig)


def grafica_error(m, color, titulo, destino):
    fig, ax = plt.subplots(figsize=(10, 4))
    ax.axhline(0, color="#c3c2b7")
    ax.plot(m["t"], m["y"] - m["yh"], color=color, linewidth=1.3)
    ax.set_title(f"{titulo}   (fit = {m['fit']:.1f} %)", loc="left")
    ax.set_ylabel("Error = medido − modelo (RPM)")
    ax.set_xlabel("Tiempo (s)")
    estilo(ax)
    fig.tight_layout()
    fig.savefig(destino, dpi=150)
    plt.close(fig)


# =========================================================================== principal
def ultima(pruebas, modo, nombre):
    sel = [p for p in pruebas if p["meta"].get("modo") == modo]
    if not sel:
        sys.exit(f"No encontre una prueba completa de {nombre} en el/los log(s).")
    return sel[-1]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--nombre", default=None)
    ap.add_argument("--sin-reporte", action="store_true")
    args = ap.parse_args()

    pruebas = []
    for ruta in args.logs:
        pruebas += leer_log(ruta)
    pe, pp = ultima(pruebas, 1, "ESCALON"), ultima(pruebas, 2, "PRBS9")
    if pe["meta"].get("quien") != pp["meta"].get("quien"):
        print("AVISO: el escalon y la PRBS9 son de integrantes distintos. Revisa los logs.")
    nombre = args.nombre or INTEGRANTES.get(pe["meta"].get("quien"), "integrante")
    carpeta = os.path.join(os.path.dirname(os.path.abspath(args.logs[0])), f"resultados_{nombre}")
    os.makedirs(carpeta, exist_ok=True)

    me, mp = modelo_escalon(pe), modelo_prbs(pp)
    sube, baja = tau_subida_bajada(mp["t"], mp["u"], mp["y"])

    grafica_datos_vs_modelo(me, AZUL, "Modelo 1: respuesta al paso",
                            f"Respuesta al escalón: datos vs modelo - {nombre}",
                            os.path.join(carpeta, "1_escalon_datos_vs_modelo.png"))
    grafica_datos_vs_modelo(mp, NARANJA, "Modelo 2: mínimos cuadrados (PRBS9)",
                            f"Respuesta a la PRBS9: datos vs modelo (primeros {VENTANA_PRBS_S:.0f} s) - {nombre}",
                            os.path.join(carpeta, "2_prbs9_datos_vs_modelo.png"), xlim=(0, VENTANA_PRBS_S))
    grafica_error(me, AZUL, f"Error del modelo 1 (escalón) - {nombre}",
                  os.path.join(carpeta, "3_error_escalon.png"))
    grafica_error(mp, NARANJA, f"Error del modelo 2 (PRBS9) - {nombre}",
                  os.path.join(carpeta, "4_error_prbs9.png"))

    rmse = lambda m: float(np.sqrt(np.mean((m["y"] - m["yh"]) ** 2)))
    claves = ("K", "tau", "a", "b", "u0", "y0", "fit")
    r = {"nombre": nombre, "bajo": pp["meta"].get("bajo"), "alto": pp["meta"].get("alto"),
         "ts_ms": me["ts"] * 1000, "cpv": pe["meta"].get("cpv"), "bitm": pp["meta"].get("bitm"),
         "n_escalon": pe["meta"]["n"], "n_prbs": pp["meta"]["n"],
         "esc": {**{k: me[k] for k in claves}, "u1": me["u1"], "y1": me["y1"],
                 "objetivo": me["objetivo"], "rmse": rmse(me)},
         "prbs": {**{k: mp[k] for k in claves}, "N": mp["N"], "rmse": rmse(mp)},
         "tau_sube": float(np.mean(sube)) if sube else None,
         "tau_baja": float(np.mean(baja)) if baja else None}
    with open(os.path.join(carpeta, "resultados.json"), "w", encoding="utf-8") as f:
        json.dump(r, f, indent=2)

    E, P = r["esc"], r["prbs"]
    texto = "\n".join([
        f"RESULTADOS - {nombre}   (Ts = {r['ts_ms']:.0f} ms)", "",
        f"MODELO 1 - Respuesta al paso (PWM {E['u0']:.0f} % -> {E['u1']:.0f} %)",
        f"  RPM {E['y0']:.1f} -> {E['y1']:.1f}",
        f"  K   = ({E['y1']:.1f} - {E['y0']:.1f}) / ({E['u1']:.0f} - {E['u0']:.0f}) = {E['K']:.4f} RPM/%",
        f"  tau = tiempo al 63.2 % ({E['objetivo']:.1f} RPM) = {E['tau'] * 1000:.1f} ms",
        f"  G1(s) = {E['K']:.4f} / ({E['tau']:.5f} s + 1)",
        f"  G1(z) = {E['b']:.4f} / (z - {E['a']:.4f})",
        f"  fit = {E['fit']:.2f} %     RMSE = {E['rmse']:.2f} RPM", "",
        f"MODELO 2 - Minimos cuadrados con PRBS9 (PWM {r['bajo']} % <-> {r['alto']} %)",
        f"  N = {P['N']} ecuaciones  ->  a = {P['a']:.4f},  b = {P['b']:.4f}",
        f"  G2(z) = {P['b']:.4f} / (z - {P['a']:.4f})",
        f"  K = b/(1-a) = {P['K']:.4f} RPM/%     tau = -Ts/ln(a) = {P['tau'] * 1000:.1f} ms",
        f"  G2(s) = {P['K']:.4f} / ({P['tau']:.5f} s + 1)",
        f"  fit = {P['fit']:.2f} %     RMSE = {P['rmse']:.2f} RPM", "",
        "TAU AL SUBIR Y AL BAJAR EL PWM (bits largos de la PRBS9)",
        (f"  sube: {r['tau_sube'] * 1000:.1f} ms     baja: {r['tau_baja'] * 1000:.1f} ms"
         if r["tau_sube"] and r["tau_baja"] else "  (no hubo suficientes bits largos)"),
        "", f"Archivos en: {carpeta}"])
    with open(os.path.join(carpeta, "resultados.txt"), "w", encoding="utf-8") as f:
        f.write(texto + "\n")
    print(texto)

    if not args.sin_reporte:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        try:
            import docx  # noqa: F401
            from generar_reporte import generar
        except ImportError:
            print("\nNo se genero el reporte .docx (falta python-docx o generar_reporte.py).")
            return
        print(f"Reporte: {generar(r, carpeta)}")


if __name__ == "__main__":
    main()
