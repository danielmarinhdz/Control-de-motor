#!/usr/bin/env python3
"""Arma el reporte (punto 5) en Word a partir de resultados.json y las graficas.

Normalmente no se corre solo: lo llama analizar_motor.py al final. Si quieres
regenerarlo sin repetir el analisis:
    python generar_reporte.py resultados_Daniel

Necesita:  python -m pip install python-docx
El texto se arma con TUS numeros. Revisalo y ajusta lo que quieras en Word.
"""
import datetime as dt
import json
import os
import sys

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

# Redaccion distinta para cada integrante (los numeros salen de cada experimento).
VARIANTES = {
    "Daniel": {
        "objetivo": ("Obtener experimentalmente el modelo dinámico del motorreductor de CD JGA25-370, "
                     "relacionando el ciclo de trabajo del PWM (entrada) con la velocidad del eje de salida "
                     "en RPM (salida), mediante dos métodos de identificación: la respuesta al paso y el "
                     "método de mínimos cuadrados con una señal de excitación PRBS9. Ambos modelos se comparan "
                     "contra los datos medidos para evaluar su exactitud con el índice de ajuste (fit)."),
        "intro_exp": ("Los experimentos se realizaron con el mismo hardware del proyecto de control de velocidad "
                      "del equipo, usando valores de PWM propios para que los datos sean distintos a los de "
                      "los demás integrantes."),
        "intro_conc": "Con base en los resultados obtenidos se concluye lo siguiente:",
    },
    "Mateo": {
        "objetivo": ("Identificar un modelo matemático de primer orden del motor de CD JGA25-370 que describa "
                     "cómo cambia su velocidad (RPM) ante cambios en el PWM aplicado. Para ello se obtuvieron "
                     "dos modelos, uno a partir de la respuesta a un escalón y otro estimando sus parámetros por "
                     "mínimos cuadrados con una secuencia PRBS9, y se cuantificó qué tan bien reproduce cada uno "
                     "los datos experimentales."),
        "intro_exp": ("Para este trabajo se utilizó el sistema embebido desarrollado por el equipo, configurado "
                      "con niveles de PWM exclusivos de este experimento personal."),
        "intro_conc": "De este experimento personal se desprenden las siguientes conclusiones:",
    },
    "Angel": {
        "objetivo": ("Caracterizar la dinámica del motorreductor JGA25-370 a partir de mediciones reales, "
                     "tomando el PWM como señal de entrada y las RPM medidas con el encoder como salida. Se busca "
                     "estimar la ganancia y la constante de tiempo del motor por dos caminos (respuesta al paso "
                     "y mínimos cuadrados con PRBS9), y comparar los modelos resultantes contra el comportamiento "
                     "medido mediante el error y el porcentaje de ajuste."),
        "intro_exp": ("Las pruebas se hicieron sobre la plataforma del proyecto del equipo; los niveles de PWM "
                      "elegidos para este experimento no coinciden con los de los otros integrantes."),
        "intro_conc": "A partir del análisis realizado se obtienen las conclusiones siguientes:",
    },
}

# --------------------------------------------------------------------------- formato
def sombrear(celda, color):
    tcPr = celda._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), color)
    tcPr.append(shd)


class Reporte:
    def __init__(self):
        self.doc = Document()
        sec = self.doc.sections[0]
        sec.page_width, sec.page_height = Inches(8.5), Inches(11)
        for lado in ("left_margin", "right_margin"):
            setattr(sec, lado, Inches(1))
        sec.top_margin = sec.bottom_margin = Inches(0.9)
        normal = self.doc.styles["Normal"]
        normal.font.name = "Calibri"
        normal.font.size = Pt(11)
        normal.paragraph_format.space_after = Pt(6)
        normal.paragraph_format.line_spacing = 1.12
        for nivel, tam in ((1, 15), (2, 12.5)):
            st = self.doc.styles[f"Heading {nivel}"]
            st.font.name = "Calibri"
            st.font.size = Pt(tam)
            st.font.bold = True
            st.font.color.rgb = RGBColor(0x1F, 0x3A, 0x5F)
            st.paragraph_format.space_before = Pt(14 if nivel == 1 else 10)
            st.paragraph_format.space_after = Pt(6)
            st.paragraph_format.keep_with_next = True
        self.n_fig = 0
        self.n_tab = 0

    def titulo(self, texto, sub):
        p = self.doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = p.add_run(texto)
        r.bold, r.font.size = True, Pt(18)
        r.font.color.rgb = RGBColor(0x1F, 0x3A, 0x5F)
        for linea in sub:
            q = self.doc.add_paragraph()
            q.alignment = WD_ALIGN_PARAGRAPH.CENTER
            q.paragraph_format.space_after = Pt(2)
            q.add_run(linea).font.size = Pt(11)
        self.doc.add_paragraph()

    def h(self, texto, nivel=1):
        self.doc.add_heading(texto, level=nivel)

    def p(self, *partes):
        """Parrafo; cada parte es texto o (texto, 'b'/'i')."""
        par = self.doc.add_paragraph()
        par.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
        for parte in partes:
            if isinstance(parte, tuple):
                r = par.add_run(parte[0])
                r.bold = "b" in parte[1]
                r.italic = "i" in parte[1]
            else:
                par.add_run(parte)
        return par

    def vi(self, texto):
        par = self.doc.add_paragraph(style="List Bullet")
        par.add_run(texto)

    def ecuacion(self, texto):
        par = self.doc.add_paragraph()
        par.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = par.add_run(texto)
        r.font.name = "Cambria Math"
        r.font.size = Pt(12)
        r._element.rPr.rFonts.set(qn("w:hAnsi"), "Cambria Math")

    def figura(self, ruta, pie):
        if not os.path.exists(ruta):
            self.p((f"[Falta la figura {os.path.basename(ruta)}]", "i"))
            return
        self.n_fig += 1
        par = self.doc.add_paragraph()
        par.alignment = WD_ALIGN_PARAGRAPH.CENTER
        par.paragraph_format.keep_with_next = True
        par.add_run().add_picture(ruta, width=Inches(6.2))
        c = self.doc.add_paragraph()
        c.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = c.add_run(f"Figura {self.n_fig}. ")
        r.bold, r.font.size = True, Pt(9.5)
        c.add_run(pie).font.size = Pt(9.5)
        return self.n_fig

    def tabla(self, encabezado, filas, pie, anchos=None):
        self.n_tab += 1
        c = self.doc.add_paragraph()
        c.alignment = WD_ALIGN_PARAGRAPH.CENTER
        c.paragraph_format.keep_with_next = True
        r = c.add_run(f"Tabla {self.n_tab}. ")
        r.bold, r.font.size = True, Pt(9.5)
        c.add_run(pie).font.size = Pt(9.5)
        t = self.doc.add_table(rows=1 + len(filas), cols=len(encabezado))
        t.style = "Table Grid"
        t.alignment = WD_TABLE_ALIGNMENT.CENTER
        for j, texto in enumerate(encabezado):
            celda = t.rows[0].cells[j]
            celda.text = ""
            run = celda.paragraphs[0].add_run(texto)
            run.bold, run.font.size = True, Pt(10)
            sombrear(celda, "DCE6F0")
        for i, fila in enumerate(filas, start=1):
            for j, texto in enumerate(fila):
                celda = t.rows[i].cells[j]
                celda.text = ""
                celda.paragraphs[0].add_run(str(texto)).font.size = Pt(10)
        for i, fila in enumerate(t.rows):         # que la tabla no se parta entre paginas
            trPr = fila._tr.get_or_add_trPr()
            no_partir = OxmlElement("w:cantSplit")
            trPr.append(no_partir)
            if i < len(t.rows) - 1:
                for celda in fila.cells:
                    for par in celda.paragraphs:
                        par.paragraph_format.keep_with_next = True
        if anchos:
            for fila in t.rows:
                for j, w in enumerate(anchos):
                    fila.cells[j].width = Inches(w)
        self.doc.add_paragraph()
        return self.n_tab




# --------------------------------------------------------------------------- contenido
def generar(r, carpeta):
    nombre = r["nombre"]
    v = VARIANTES.get(nombre, VARIANTES["Daniel"])
    E, P = r["esc"], r["prbs"]
    ts, bajo, alto = r["ts_ms"], r["bajo"], r["alto"]
    rpm_por_cuenta = 60.0 / (r["cpv"] * ts / 1000.0)
    fig = lambda archivo: os.path.join(carpeta, archivo)

    R = Reporte()
    R.titulo("Modelado de un motor de CD: respuesta al paso y mínimos cuadrados con PRBS9",
             ["Microcontroladores y control - Tec de Monterrey, Campus Juárez",
              f"Experimento personal de {nombre}", dt.date.today().strftime("%d/%m/%Y")])

    # 1. Objetivo
    R.h("1. Objetivo")
    R.p(v["objetivo"])

    # 2. Experimentos
    R.h("2. Descripción de los experimentos")
    R.p(v["intro_exp"])
    R.vi("Microcontrolador NUCLEO-F446RE programado en C con HAL.")
    R.vi("Puente H BTS7960 con PWM de 20 kHz (TIM1).")
    R.vi("Motorreductor JGA25-370 a 12 V, sin carga.")
    R.vi(f"Encoder leído con TIM3; {r['cpv']} cuentas por vuelta del eje de salida.")
    R.vi(f"Muestreo cada {ts:.0f} ms con TIM2. Los datos se guardan en la RAM y al final se mandan por "
         "USART2 a la computadora, donde se procesan con Python.")
    R.p("La velocidad se calcula con las cuentas del encoder entre dos muestras:")
    R.ecuacion(f"RPM(k) = [c(k) − c(k−1)] / {r['cpv']} · 60 / Ts")
    R.p(f"Con Ts = {ts:.0f} ms, una cuenta equivale a {rpm_por_cuenta:.2f} RPM, por eso las curvas medidas "
        "tienen un pequeño escalonado.")
    R.p(f"Valores de PWM usados en este experimento (distintos a los de los demás integrantes): "
        f"nivel bajo {bajo} % y nivel alto {alto} %.")

    R.h("2.1 Experimento 1: respuesta al escalón", 2)
    R.p(f"El motor se mantuvo apagado 1 s y después se aplicó un escalón de 0 % a {alto} % de PWM durante 4 s "
        f"({r['n_escalon']} muestras).")
    R.h("2.2 Experimento 2: señal PRBS9", 2)
    R.p(f"Se aplicó una secuencia binaria pseudoaleatoria de 9 bits (PRBS9), generada en el microcontrolador "
        f"con un registro de corrimiento de polinomio x⁹ + x⁵ + 1. La secuencia tiene 511 bits; cada bit dura "
        f"{r['bitm'] * ts:.0f} ms y cambia el PWM entre {bajo} % (bit 0) y {alto} % (bit 1). Antes de la "
        f"secuencia el motor estuvo 1.5 s en {bajo} % para estabilizarse ({r['n_prbs']} muestras en total).")
    R.p("En las Figuras 1 y 2 se observan las señales de entrada (PWM, parte superior) y de salida (RPM "
        "medidas, curva gris) de ambos experimentos.")

    # 3. Modelos
    R.h("3. Modelos obtenidos")
    R.p("Se usa un modelo de primer orden:")
    R.ecuacion("G(s) = Y(s) / U(s) = K / (τs + 1)")
    R.h("3.1 Modelo 1: respuesta al paso", 2)
    R.ecuacion(f"K = ({E['y1']:.1f} − {E['y0']:.1f}) RPM / ({E['u1']:.0f} − {E['u0']:.0f}) % = {E['K']:.4f} RPM/%")
    R.p(f"El 63.2 % del cambio es {E['objetivo']:.1f} RPM y se alcanza {E['tau'] * 1000:.1f} ms después del "
        f"escalón, por lo que τ = {E['tau'] * 1000:.1f} ms:")
    R.ecuacion(f"G₁(s) = {E['K']:.4f} / ({E['tau']:.5f} s + 1)")
    R.ecuacion(f"G₁(z) = {E['b']:.4f} / (z − {E['a']:.4f})     (Ts = {ts:.0f} ms)")

    R.h("3.2 Modelo 2: mínimos cuadrados con PRBS9", 2)
    R.p("Se propone el modelo discreto, usando desviaciones respecto al promedio de la prueba:")
    R.ecuacion("y(k) = a·y(k−1) + b·u(k−1)")
    R.p(f"Con las {P['N']} muestras se arma Y = Φθ, donde cada renglón de Φ es [y(k−1)  u(k−1)] y "
        "θ = [a  b]ᵀ. La solución por mínimos cuadrados es:")
    R.ecuacion("θ = (ΦᵀΦ)⁻¹ ΦᵀY")
    R.p(f"Se obtuvo a = {P['a']:.4f} y b = {P['b']:.4f}:")
    R.ecuacion(f"G₂(z) = {P['b']:.4f} / (z − {P['a']:.4f})")
    R.ecuacion(f"K = b / (1 − a) = {P['K']:.4f} RPM/%,     τ = −Ts / ln(a) = {P['tau'] * 1000:.1f} ms")
    R.ecuacion(f"G₂(s) = {P['K']:.4f} / ({P['tau']:.5f} s + 1)")
    R.tabla(["", "Modelo 1 (escalón)", "Modelo 2 (PRBS9)"],
            [["K (RPM/%)", f"{E['K']:.4f}", f"{P['K']:.4f}"],
             ["τ (ms)", f"{E['tau'] * 1000:.1f}", f"{P['tau'] * 1000:.1f}"]],
            "Parámetros de los dos modelos.", anchos=[1.6, 1.8, 1.8])

    # 4. Analisis
    R.h("4. Análisis de los resultados")
    R.p("Cada modelo se simuló con el PWM medido y se comparó con las RPM medidas. El ajuste se calcula con:")
    R.ecuacion("fit = 100 · (1 − ‖y − ŷ‖ / ‖y − ȳ‖)  %")
    R.figura(fig("1_escalon_datos_vs_modelo.png"), "Escalón: PWM aplicado, RPM medidas y modelo 1.")
    R.figura(fig("2_prbs9_datos_vs_modelo.png"), "PRBS9 (primeros 6 s): PWM aplicado, RPM medidas y modelo 2.")
    R.figura(fig("3_error_escalon.png"), "Error entre el modelo 1 y los datos del escalón.")
    R.figura(fig("4_error_prbs9.png"), "Error entre el modelo 2 y los datos de la PRBS9.")
    R.tabla(["Modelo", "Fit", "RMSE (RPM)"],
            [["Modelo 1 (escalón)", f"{E['fit']:.2f} %", f"{E['rmse']:.2f}"],
             ["Modelo 2 (PRBS9)", f"{P['fit']:.2f} %", f"{P['rmse']:.2f}"]],
            "Ajuste y error de cada modelo con sus datos.", anchos=[2.0, 1.4, 1.4])

    R.h("4.1 Discusión", 2)
    R.p(f"El modelo del escalón tuvo un fit de {E['fit']:.1f} % y el de mínimos cuadrados {P['fit']:.1f} %. "
        "En las gráficas de error, el error grande aparece justo cuando cambia el PWM; en régimen permanente "
        f"solo queda el escalonado del encoder (±{rpm_por_cuenta / 2:.1f} RPM).")
    R.p(f"Las ganancias son distintas ({E['K']:.2f} contra {P['K']:.2f} RPM/%) porque el escalón parte del "
        "motor detenido e incluye la zona muerta, mientras que la PRBS9 trabaja entre dos velocidades sin que "
        "el motor se detenga.")
    if r.get("tau_sube") and r.get("tau_baja"):
        s_, b_ = r["tau_sube"] * 1000, r["tau_baja"] * 1000
        R.p(f"Las constantes de tiempo también difieren ({E['tau'] * 1000:.0f} contra {P['tau'] * 1000:.0f} ms). "
            f"Midiendo en la PRBS9 los cambios largos, el motor tarda τ ≈ {s_:.0f} ms al subir el PWM y "
            f"τ ≈ {b_:.0f} ms al bajarlo. El escalón solo mide una subida, mientras que mínimos cuadrados "
            "promedia subidas y bajadas."
            + (" Esta diferencia entre subir y bajar no la puede representar un modelo de primer orden, y es "
               "la principal fuente de error en la PRBS9." if max(s_, b_) / min(s_, b_) > 1.3 else ""))

    # 5. Conclusiones
    R.h("5. Conclusiones")
    R.p(v["intro_conc"])
    R.vi(f"Por respuesta al paso se obtuvo G₁(s) = {E['K']:.3f}/({E['tau']:.4f}s + 1), con un fit de "
         f"{E['fit']:.1f} %.")
    R.vi(f"Por mínimos cuadrados con PRBS9 se obtuvo G₂(s) = {P['K']:.3f}/({P['tau']:.4f}s + 1), con un fit de "
         f"{P['fit']:.1f} %.")
    R.vi("El método del escalón es más sencillo; el de mínimos cuadrados usa todas las muestras y da directamente "
         "el modelo discreto.")
    R.vi("Estos modelos servirán para sintonizar el PID de velocidad del proyecto.")

    destino = os.path.join(carpeta, f"Reporte_{nombre}.docx")
    R.doc.save(destino)
    return destino


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Uso: python generar_reporte.py resultados_<Nombre>")
    with open(os.path.join(sys.argv[1], "resultados.json"), encoding="utf-8") as f:
        print(generar(json.load(f), sys.argv[1]))
