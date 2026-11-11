#!/usr/bin/env python3
"""Analisis completo del experimento del motor (puntos 1 a 5 de la actividad).

Uso (desde la carpeta Experimento_PWM):
    python analizar_motor.py datos_Daniel.log --nombre Daniel

Lee del log la ultima prueba completa de ESCALON y la ultima de PRBS9 y genera, en la
carpeta resultados_<Nombre>/:

  Punto 1  1_senales_escalon.png        PWM (entrada) y RPM (salida) de la prueba del escalon
           2_senales_prbs9.png          PWM y RPM de la prueba PRBS9 completa
           3_senales_prbs9_zoom.png     los primeros 3 s de la PRBS9, para verla de cerca
  Punto 2  modelo 1: respuesta al paso   G(s) = K / (tau s + 1)
             K   = (RPM final - RPM inicial) / (PWM final - PWM inicial)
             tau = tiempo en llegar al 63.2 % del cambio
           modelo 2: minimos cuadrados con la PRBS9 (modelo ARX de primer orden)
             y(k) = a y(k-1) + b u(k-1)   ->   theta = [a b]^T = (Phi^T Phi)^-1 Phi^T Y
             G(z) = b / (z - a),  K = b / (1 - a),  tau = -Ts / ln(a)
  Punto 3  4_comparacion_escalon.png    datos del escalon vs los dos modelos
           5_comparacion_prbs9.png      datos de la PRBS9 vs los dos modelos
           6_comparacion_prbs9_zoom.png lo mismo, 3 s de cerca
  Punto 4  7_error_escalon.png / 8_error_prbs9.png   error = medido - modelo, y fit
             fit = 100 (1 - ||y - y_modelo|| / ||y - media(y)||)   (100 % = perfecto)
  Punto 5  Reporte_<Nombre>.docx        reporte armado con las graficas y los numeros
           resultados.txt                todos los numeros en texto

Notas:
  - Los modelos se SIMULAN usando solo el PWM medido (no ven las RPM), asi el fit es honesto.
  - Ambos modelos son lineales alrededor del punto de operacion (entre el nivel BAJO y el ALTO):
    se trabaja con desviaciones (u - u0, y - y0), lo que deja fuera la zona muerta del motor.
"""
import argparse
import json
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

GRIS = "#52514e"       # datos medidos / texto
TINTA = "#0b0b0b"
AZUL = "#2a78d6"       # modelo 1 (escalon)
NARANJA = "#eb6834"    # modelo 2 (PRBS9, minimos cuadrados)
REJILLA = "#e8e7e2"
EJE = "#c3c2b7"

INTEGRANTES = {1: "Daniel", 2: "Mateo", 3: "Angel"}
ANTES_DEL_SALTO_S = 1.0      # cuanto del nivel BAJO se muestra antes del salto
REGIMEN_BAJO_S = 1.5         # ultimos 1.5 s en BAJO = regimen permanente inicial
REGIMEN_ALTO_S = 2.0         # ultimos 2.0 s en ALTO = regimen permanente final
ZOOM_S = 3.0


# =========================================================================== lectura
def leer_log(ruta):
    """Lista de pruebas completas del log. Cada una: {meta, pwm (array), cuenta (array)}."""
    pruebas, meta, pwm, cuenta = [], None, [], []
    with open(ruta, encoding="utf-8", errors="replace") as f:
        for linea in f:
            partes = linea.strip().split(",")
            if len(partes) < 2 or partes[0] != "EXP":
                continue
            campo = partes[1]
            if campo == "INICIO":
                meta = {}
                for p in partes[2:]:
                    if "=" in p:
                        k, v = p.split("=", 1)
                        try:
                            meta[k] = int(v)
                        except ValueError:
                            pass
                pwm, cuenta = [], []
            elif campo == "FIN":
                if meta is not None and len(pwm) == meta.get("n", -1):
                    pruebas.append({"meta": meta, "pwm": np.array(pwm, float),
                                    "cuenta": np.array(cuenta, np.int64)})
                meta = None
            elif campo.isdigit() and meta is not None and len(partes) == 4:
                try:
                    pwm.append(int(partes[2]))
                    cuenta.append(int(partes[3]))
                except ValueError:
                    meta["corrupta"] = 1
    return [p for p in pruebas if not p["meta"].get("corrupta")]


def senales(prueba, m=1):
    """Convierte cuentas en RPM sobre ventanas de m ciclos de 10 ms.
    Devuelve t (fin de cada ventana, s), u (PWM aplicado durante la ventana, %),
    y (velocidad media en la ventana, RPM) y Ts (s)."""
    meta = prueba["meta"]
    ts_base = meta.get("ts_ms", 10) / 1000.0
    cpv, sgn = float(meta.get("cpv", 685)), float(meta.get("sgn", -1))
    idx = np.arange(0, len(prueba["cuenta"]), m)
    d = np.diff(prueba["cuenta"][idx])
    d = ((d + 32768) % 65536) - 32768                  # desbordamiento del contador de 16 bits
    ts = m * ts_base
    y = sgn * d / cpv * 60.0 / ts
    u = np.array([prueba["pwm"][idx[j - 1]:idx[j]].mean() for j in range(1, len(idx))])
    return idx[1:] * ts_base, u, y, ts


# =========================================================================== modelos
def simular(a, b, u, y_med, u0, y0):
    """y(k) = a y(k-1) + b u(k-1) en desviaciones respecto a (u0, y0).
    u[j] es el PWM aplicado DURANTE la ventana j, o sea u(k-1) para y(k).
    Arranca del valor medido y_med[0]. Devuelve RPM absolutas."""
    du = u - u0
    yh = np.empty(len(u))
    yh[0] = y_med[0] - y0
    for j in range(1, len(u)):
        yh[j] = a * yh[j - 1] + b * du[j]
    return yh + y0


def fit(y, yh):
    return 100.0 * (1.0 - np.linalg.norm(y - yh) / np.linalg.norm(y - y.mean()))


def modelo_escalon(prueba, m):
    t, u, y, ts = senales(prueba, m)
    meta = prueba["meta"]
    t_salto = meta["ini"] * meta.get("ts_ms", 10) / 1000.0
    antes = (t <= t_salto + 1e-9) & (t > t_salto - REGIMEN_BAJO_S + 1e-9)
    final = t > t[-1] - REGIMEN_ALTO_S + 1e-9
    u0, y0 = u[antes].mean(), y[antes].mean()
    u1, y1 = u[final].mean(), y[final].mean()
    K = (y1 - y0) / (u1 - u0)

    objetivo = y0 + 0.632 * (y1 - y0)
    tau, t_cruce = None, None
    for j in np.where(t > t_salto + 1e-9)[0]:
        if y[j] >= objetivo:
            t_prev, y_prev = (t[j - 1], y[j - 1])
            t_cruce = t_prev + (objetivo - y_prev) / (y[j] - y_prev) * (t[j] - t_prev)
            tau = max(t_cruce - t_salto, 1e-4)
            break
    if tau is None:
        sys.exit("La salida del escalon no llego al 63.2 % del cambio: revisa la prueba.")

    a = float(np.exp(-ts / tau))
    b = float(K * (1.0 - a))
    # 10-90 % y establecimiento (2 %) medidos, para el analisis
    def cruce(fr):
        obj = y0 + fr * (y1 - y0)
        for j in np.where(t > t_salto + 1e-9)[0]:
            if y[j] >= obj:
                return t[j - 1] + (obj - y[j - 1]) / (y[j] - y[j - 1]) * (t[j] - t[j - 1]) - t_salto
        return float("nan")
    seg = t >= t_salto - ANTES_DEL_SALTO_S - 1e-9
    return {"K": float(K), "tau": float(tau), "a": a, "b": b, "ts": ts,
            "u0": float(u0), "y0": float(y0), "u1": float(u1), "y1": float(y1),
            "t_salto": t_salto, "objetivo_632": float(objetivo), "t_cruce": float(t_cruce),
            "t_subida_10_90": float(cruce(0.9) - cruce(0.1)),
            "ruido_regimen": float(y[final].std()),
            "todo": (t, u, y), "seg": (t[seg] - t_salto, u[seg], y[seg])}


def modelo_prbs(prueba, m):
    t, u, y, ts = senales(prueba, m)
    meta = prueba["meta"]
    t_ini = meta["ini"] * meta.get("ts_ms", 10) / 1000.0
    seg = (t - ts) >= t_ini - 1e-9                  # ventanas dentro de la secuencia
    t, u, y = t[seg] - t_ini, u[seg], y[seg]
    u0, y0 = u.mean(), y.mean()
    ud, yd = u - u0, y - y0
    Phi = np.column_stack([yd[:-1], ud[1:]])         # [y(k-1)  u(k-1)]
    Y = yd[1:]                                       #  y(k)
    theta = np.linalg.solve(Phi.T @ Phi, Phi.T @ Y)  # (Phi^T Phi)^-1 Phi^T Y
    a, b = float(theta[0]), float(theta[1])
    if not 0.0 < a < 1.0:
        print(f"AVISO: a = {a:.4f} no esta entre 0 y 1; el modelo de primer orden no es valido.")
    K = b / (1.0 - a)
    tau = -ts / np.log(a) if 0.0 < a < 1.0 else float("nan")
    residuo = Y - Phi @ theta
    return {"K": float(K), "tau": float(tau), "a": a, "b": b, "ts": ts,
            "u0": float(u0), "y0": float(y0), "N": int(len(Y)),
            "rmse_1paso": float(np.sqrt(np.mean(residuo ** 2))),
            "todo": senales(prueba, m)[:3], "t_ini": t_ini, "seg": (t, u, y)}


def tau_por_sentido(t, u, y, minimo=18):
    """Constante de tiempo al 63.2 % en cada bit LARGO de la PRBS9 (>= minimo ventanas antes y
    despues del cambio), separando subidas y bajadas de PWM. Devuelve (taus_sube, taus_baja) en s."""
    cambios = np.where(np.diff(u) != 0)[0] + 1
    sube, baja = [], []
    for i in range(1, len(cambios) - 1):
        c, antes, despues = cambios[i], cambios[i] - cambios[i - 1], cambios[i + 1] - cambios[i]
        if antes < minimo or despues < minimo:
            continue
        y0, y1 = y[c - 3:c].mean(), y[cambios[i + 1] - 5:cambios[i + 1]].mean()
        objetivo = y0 + 0.632 * (y1 - y0)
        signo = np.sign(y1 - y0)
        for j in range(c, cambios[i + 1]):
            if (y[j] - objetivo) * signo >= 0:
                f = (objetivo - y[j - 1]) / (y[j] - y[j - 1]) if y[j] != y[j - 1] else 1.0
                tau = (t[j - 1] + f * (t[j] - t[j - 1])) - t[c - 1]
                (sube if y1 > y0 else baja).append(tau)
                break
    return sube, baja


# =========================================================================== graficas
def estilo(ax):
    for lado in ("top", "right"):
        ax.spines[lado].set_visible(False)
    for lado in ("left", "bottom"):
        ax.spines[lado].set_color(EJE)
    ax.tick_params(colors=GRIS)
    ax.grid(True, color=REJILLA, linewidth=0.8)
    ax.set_axisbelow(True)


def titulo(ax, texto):
    ax.set_title(texto, loc="left", color=TINTA, fontsize=13)


def fig_senales(t, u, y, texto, destino, xlim=None, marcas=None):
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(10, 6), sharex=True,
                                 gridspec_kw={"height_ratios": [1, 1.6]})
    a1.step(t, u, where="pre", color=GRIS, linewidth=2)
    a1.set_ylabel("PWM aplicado (%)", color=GRIS)
    a1.set_ylim(-3, 100)
    a2.plot(t, y, color=AZUL, linewidth=1.8)
    a2.set_ylabel("Velocidad medida (RPM)", color=GRIS)
    a2.set_xlabel("Tiempo (s)", color=GRIS)
    titulo(a1, texto)
    for a in (a1, a2):
        estilo(a)
        if xlim:
            a.set_xlim(*xlim)
    if marcas:
        for x, etiqueta in marcas:
            for a in (a1, a2):
                a.axvline(x, color=EJE, linewidth=1, linestyle=":")
            a1.annotate(etiqueta, (x, 96), xytext=(4, 0), textcoords="offset points",
                        color=GRIS, fontsize=9, va="top")
    fig.tight_layout()
    fig.savefig(destino, dpi=160)
    plt.close(fig)


def fig_comparacion(t, u, y, yh1, yh2, f1, f2, texto, destino, xlim=None, escalon=False):
    fig, (a0, a1) = plt.subplots(2, 1, figsize=(10, 6.5), sharex=True,
                                 gridspec_kw={"height_ratios": [0.8, 2]})
    a0.step(t, u, where="pre", color=GRIS, linewidth=2)
    a0.set_ylabel("PWM (%)", color=GRIS)
    titulo(a0, texto)
    a1.plot(t, y, color=GRIS, linewidth=1.4, alpha=0.75, label="Datos experimentales",
            marker="o" if escalon else None, markersize=3)
    a1.plot(t, yh1, color=AZUL, linewidth=2.2, label=f"Modelo 1: respuesta al paso (fit {f1:.1f} %)")
    a1.plot(t, yh2, color=NARANJA, linewidth=2.2, linestyle="--",
            label=f"Modelo 2: mínimos cuadrados PRBS9 (fit {f2:.1f} %)")
    a1.set_ylabel("Velocidad (RPM)", color=GRIS)
    a1.set_xlabel("Tiempo (s)" + (" desde el salto de PWM" if escalon else " desde el inicio de la PRBS9"), color=GRIS)
    a1.legend(frameon=False, loc="lower left", bbox_to_anchor=(0.0, 1.0), ncol=3, labelcolor=GRIS, fontsize=8.5,
              handlelength=2.2, columnspacing=1.2)
    for a in (a0, a1):
        estilo(a)
    if xlim:
        a1.set_xlim(*xlim)
    fig.tight_layout()
    fig.savefig(destino, dpi=160)
    plt.close(fig)


def fig_error(t, e1, e2, f1, f2, texto, destino, escalon=False, zoom=None):
    filas = 2 if zoom else 1
    fig, ejes = plt.subplots(filas, 1, figsize=(10, 4.2 * filas), squeeze=False)
    eje_x = "Tiempo (s)" + (" desde el salto de PWM" if escalon else " desde el inicio de la PRBS9")
    for i, ax in enumerate(ejes[:, 0]):
        ax.axhline(0, color=EJE, linewidth=1)
        ax.plot(t, e1, color=AZUL, linewidth=1.5, label=f"Modelo 1: respuesta al paso (fit {f1:.1f} %)")
        ax.plot(t, e2, color=NARANJA, linewidth=1.5, linestyle="--",
                label=f"Modelo 2: mínimos cuadrados PRBS9 (fit {f2:.1f} %)")
        ax.set_ylabel("Error = medido − modelo (RPM)", color=GRIS)
        ax.set_xlabel(eje_x + (f" (acercamiento a los primeros {zoom:.0f} s)" if i == 1 else ""), color=GRIS)
        estilo(ax)
        if i == 1:
            ax.set_xlim(0, zoom)
    titulo(ejes[0, 0], texto)
    manijas, etiquetas = ejes[0, 0].get_legend_handles_labels()
    fig.legend(manijas, etiquetas, frameon=False, loc="lower center", ncol=2, labelcolor=GRIS, fontsize=9)
    fig.tight_layout(rect=(0, 0.06 / filas, 1, 1))
    fig.savefig(destino, dpi=160)
    plt.close(fig)


# =========================================================================== principal
def ultima(pruebas, modo, nombre_modo):
    sel = [p for p in pruebas if p["meta"].get("modo") == modo]
    if not sel:
        sys.exit(f"No encontre una prueba completa de {nombre_modo} en el/los log(s). "
                 "Repite la captura (escalon = 1a pulsacion, PRBS9 = 2a pulsacion).")
    p = sel[-1]
    if p["meta"].get("perdidas", 0):
        print(f"AVISO: en la prueba de {nombre_modo} se interpolaron {p['meta']['perdidas']} ciclos de 10 ms "
              "(el lazo se atraso). Si son mas de ~20, repite la prueba.")
    return p


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+", help="log(s) con las pruebas (capturar_serial.py)")
    ap.add_argument("--nombre", default=None, help="Daniel, Mateo o Angel (si no, se toma del log)")
    ap.add_argument("--m", type=int, default=1, help="ventana para calcular RPM en ciclos de 10 ms (1 = 10 ms)")
    ap.add_argument("--sin-reporte", action="store_true", help="no generar el .docx")
    args = ap.parse_args()

    pruebas = []
    for ruta in args.logs:
        pruebas += leer_log(ruta)
    pe = ultima(pruebas, 1, "ESCALON")
    pp = ultima(pruebas, 2, "PRBS9")
    quien = pe["meta"].get("quien")
    if pp["meta"].get("quien") != quien or pp["meta"].get("bajo") != pe["meta"].get("bajo"):
        print("AVISO: la prueba del escalon y la de PRBS9 no tienen el mismo integrante/niveles de PWM.")
    nombre = args.nombre or INTEGRANTES.get(quien, "integrante")
    carpeta = os.path.join(os.path.dirname(os.path.abspath(args.logs[0])), f"resultados_{nombre}")
    os.makedirs(carpeta, exist_ok=True)
    ruta = lambda archivo: os.path.join(carpeta, archivo)

    me = modelo_escalon(pe, args.m)
    mp = modelo_prbs(pp, args.m)
    ts = me["ts"]
    bajo, alto = pe["meta"].get("bajo"), pe["meta"].get("alto")

    # ---------------- Punto 1: senales experimentales
    t, u, y = me["todo"]
    fig_senales(t, u, y, f"Prueba del escalón: entrada PWM y salida RPM - {nombre}", ruta("1_senales_escalon.png"),
                marcas=[(1.0, f"0 → {bajo} %"), (me["t_salto"], f"{bajo} → {alto} %")])
    t, u, y = mp["todo"]
    fig_senales(t, u, y, f"Prueba PRBS9: entrada PWM y salida RPM - {nombre}", ruta("2_senales_prbs9.png"),
                marcas=[(mp["t_ini"], "inicia PRBS9")])
    fig_senales(t - mp["t_ini"], u, y, f"Prueba PRBS9, primeros {ZOOM_S:.0f} s de la secuencia - {nombre}",
                ruta("3_senales_prbs9_zoom.png"), xlim=(-0.2, ZOOM_S))

    # ---------------- Punto 3 y 4: cada modelo simulado con el PWM de cada prueba
    te, ue, ye = me["seg"]
    o_e = (me["u0"], me["y0"])                      # punto de operacion: regimen en BAJO
    yh_ee = simular(me["a"], me["b"], ue, ye, *o_e)
    yh_pe = simular(mp["a"], mp["b"], ue, ye, *o_e)
    tp, up, yp = mp["seg"]
    o_p = (mp["u0"], mp["y0"])                      # punto de operacion: promedio de la PRBS9
    yh_ep = simular(me["a"], me["b"], up, yp, *o_p)
    yh_pp = simular(mp["a"], mp["b"], up, yp, *o_p)
    f = {"ee": fit(ye, yh_ee), "pe": fit(ye, yh_pe), "ep": fit(yp, yh_ep), "pp": fit(yp, yh_pp)}

    fig_comparacion(te, ue, ye, yh_ee, yh_pe, f["ee"], f["pe"],
                    f"Respuesta al escalón: datos vs modelos - {nombre}", ruta("4_comparacion_escalon.png"),
                    xlim=(-0.3, 1.0), escalon=True)
    fig_comparacion(tp, up, yp, yh_ep, yh_pp, f["ep"], f["pp"],
                    f"Prueba PRBS9: datos vs modelos - {nombre}", ruta("5_comparacion_prbs9.png"))
    fig_comparacion(tp, up, yp, yh_ep, yh_pp, f["ep"], f["pp"],
                    f"Prueba PRBS9: datos vs modelos, primeros {ZOOM_S:.0f} s - {nombre}",
                    ruta("6_comparacion_prbs9_zoom.png"), xlim=(0, ZOOM_S))
    fig_error(te, ye - yh_ee, ye - yh_pe, f["ee"], f["pe"],
              f"Error entre modelos y datos: prueba del escalón - {nombre}", ruta("7_error_escalon.png"), escalon=True)
    fig_error(tp, yp - yh_ep, yp - yh_pp, f["ep"], f["pp"],
              f"Error entre modelos y datos: prueba PRBS9 - {nombre}", ruta("8_error_prbs9.png"), zoom=ZOOM_S)

    err = lambda y_, yh_: (float(np.sqrt(np.mean((y_ - yh_) ** 2))), float(np.max(np.abs(y_ - yh_))))
    r = {
        "nombre": nombre, "bajo": bajo, "alto": alto, "ts_ms": ts * 1000, "m": args.m,
        "cpv": pe["meta"].get("cpv"), "bitm": pp["meta"].get("bitm"),
        "n_escalon": int(pe["meta"]["n"]), "n_prbs": int(pp["meta"]["n"]),
        "perdidas": [pe["meta"].get("perdidas", 0), pp["meta"].get("perdidas", 0)],
        "y_cero_bajo": float(me["todo"][2][(me["todo"][0] > 3.0) & (me["todo"][0] <= 4.0)].mean()),
        "esc": {k: me[k] for k in ("K", "tau", "a", "b", "u0", "y0", "u1", "y1", "objetivo_632",
                                   "t_subida_10_90", "ruido_regimen")},
        "prbs": {k: mp[k] for k in ("K", "tau", "a", "b", "u0", "y0", "N", "rmse_1paso")},
        "fit": f,
        "error": {"ee": err(ye, yh_ee), "pe": err(ye, yh_pe), "ep": err(yp, yh_ep), "pp": err(yp, yh_pp)},
        "y_prbs_min": float(yp.min()), "y_prbs_max": float(yp.max()),
    }
    ts_sube, ts_baja = tau_por_sentido(tp, up, yp)
    r["asimetria"] = {"tau_sube": float(np.mean(ts_sube)) if ts_sube else None,
                      "tau_baja": float(np.mean(ts_baja)) if ts_baja else None,
                      "n_sube": len(ts_sube), "n_baja": len(ts_baja)}
    with open(ruta("resultados.json"), "w", encoding="utf-8") as fjs:
        json.dump(r, fjs, indent=2)

    E, P = r["esc"], r["prbs"]
    L = [f"RESULTADOS - {nombre}   (PWM BAJO = {bajo} %, ALTO = {alto} %, Ts = {ts * 1000:.0f} ms)", "",
         "MODELO 1 - Respuesta al paso (salto de PWM BAJO -> ALTO)",
         f"  PWM {E['u0']:.0f} % -> {E['u1']:.0f} %     RPM {E['y0']:.1f} -> {E['y1']:.1f}",
         f"  K   = ({E['y1']:.1f} - {E['y0']:.1f}) / ({E['u1']:.0f} - {E['u0']:.0f}) = {E['K']:.4f} RPM/%",
         f"  tau = tiempo al 63.2 % ({E['objetivo_632']:.1f} RPM) = {E['tau'] * 1000:.1f} ms",
         f"  G1(s) = {E['K']:.4f} / ({E['tau']:.5f} s + 1)",
         f"  G1(z) = {E['b']:.4f} / (z - {E['a']:.4f})      (ZOH, Ts = {ts * 1000:.0f} ms)", "",
         "MODELO 2 - Minimos cuadrados con PRBS9:  y(k) = a y(k-1) + b u(k-1)",
         f"  N = {P['N']} ecuaciones,  theta = (Phi^T Phi)^-1 Phi^T Y  ->  a = {P['a']:.4f},  b = {P['b']:.4f}",
         f"  G2(z) = {P['b']:.4f} / (z - {P['a']:.4f})",
         f"  K = b/(1-a) = {P['K']:.4f} RPM/%     tau = -Ts/ln(a) = {P['tau'] * 1000:.1f} ms",
         f"  G2(s) = {P['K']:.4f} / ({P['tau']:.5f} s + 1)", "",
         "AJUSTE (fit %) - cada modelo se simula solo con el PWM",
         "                        datos escalon    datos PRBS9",
         f"  Modelo 1 (escalon)      {f['ee']:7.2f}         {f['ep']:7.2f}",
         f"  Modelo 2 (PRBS9)        {f['pe']:7.2f}         {f['pp']:7.2f}",
         "", "ERROR (RMSE / maximo, RPM)",
         f"  Modelo 1: escalon {r['error']['ee'][0]:.2f} / {r['error']['ee'][1]:.1f}   PRBS9 {r['error']['ep'][0]:.2f} / {r['error']['ep'][1]:.1f}",
         f"  Modelo 2: escalon {r['error']['pe'][0]:.2f} / {r['error']['pe'][1]:.1f}   PRBS9 {r['error']['pp'][0]:.2f} / {r['error']['pp'][1]:.1f}",
         ""]
    A_ = r["asimetria"]
    if A_["tau_sube"] and A_["tau_baja"]:
        L += ["SUBIDA vs BAJADA (bits largos de la PRBS9, tau al 63.2 %)",
              f"  al SUBIR el PWM: tau = {A_['tau_sube'] * 1000:.1f} ms ({A_['n_sube']} cambios)   "
              f"al BAJAR el PWM: tau = {A_['tau_baja'] * 1000:.1f} ms ({A_['n_baja']} cambios)", ""]
    L += [f"Graficas y reporte en: {carpeta}"]
    texto = "\n".join(L)
    with open(ruta("resultados.txt"), "w", encoding="utf-8") as ftx:
        ftx.write(texto + "\n")
    print(texto)

    if not args.sin_reporte:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        try:
            import docx  # noqa: F401
        except ImportError:
            print("\nNo se genero el reporte .docx: falta python-docx. Instala:  python -m pip install python-docx")
            return
        from generar_reporte import generar
        destino = generar(r, carpeta)
        print(f"Reporte: {destino}")


if __name__ == "__main__":
    main()
