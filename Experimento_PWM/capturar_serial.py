#!/usr/bin/env python3
"""Captura los datos que manda la Nucleo por USART2 y los guarda en un archivo .log.

Uso (Windows, desde la carpeta Experimento_PWM):
    python capturar_serial.py COM12 --salida datos_Daniel.log

Despues de lanzar el comando:
    1. Presiona el boton azul  -> prueba del ESCALON (8 s) y luego manda los datos.
    2. Espera a "Prueba completa (1 de 2)".
    3. Presiona el boton azul otra vez -> prueba PRBS9 (32 s) y luego manda los datos.
    4. Al llegar a "Prueba completa (2 de 2)" el script se cierra solo.
Ctrl+C termina antes.
"""
import argparse
import os
import sys

try:
    import serial
except ImportError:
    sys.exit("Falta pyserial. Instala con:  python -m pip install pyserial")

NOMBRE_MODO = {"1": "ESCALON", "2": "PRBS9"}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("puerto", help="puerto de la Nucleo, p. ej. COM12")
    ap.add_argument("--salida", required=True, help="archivo de salida, p. ej. datos_Daniel.log")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--pruebas", type=int, default=2, help="terminar tras N pruebas completas (por defecto 2)")
    ap.add_argument("--sobrescribir", action="store_true", help="reemplazar el archivo si ya existe")
    args = ap.parse_args()

    if os.path.exists(args.salida) and not args.sobrescribir:
        sys.exit(f"{args.salida} ya existe. Usa otro nombre o agrega --sobrescribir para reemplazarlo.")

    try:
        ser = serial.Serial(args.puerto, args.baud, timeout=1)
    except serial.SerialException as e:
        sys.exit(f"No pude abrir {args.puerto}: {e}\n"
                 "Revisa el numero de COM y cierra cualquier otro monitor serie (VS Code, PuTTY...).")

    completas, muestras = 0, 0
    print(f"Escuchando {args.puerto} a {args.baud}. Guardando en {args.salida}")
    print("Presiona el boton azul de la Nucleo para empezar la prueba del ESCALON.")

    with ser, open(args.salida, "w", encoding="utf-8") as f:
        try:
            while completas < args.pruebas:
                linea = ser.readline().decode("ascii", errors="replace").strip()
                if not linea.startswith("EXP,"):
                    continue
                f.write(linea + "\n")
                f.flush()
                campo = linea.split(",")[1]
                if campo.isdigit():
                    muestras += 1
                    if muestras % 500 == 0:
                        print(f"  recibiendo datos... {muestras} muestras")
                    continue
                if campo == "CORRIENDO":
                    print("Prueba en curso (el motor esta girando)...")
                elif campo == "INICIO":
                    modo = dict(p.split("=") for p in linea.split(",")[2:] if "=" in p).get("modo", "?")
                    print(f"La placa esta mandando los datos de la prueba {NOMBRE_MODO.get(modo, modo)}...")
                    muestras = 0
                elif campo == "ABORT":
                    print("Prueba abortada (se presiono el boton a la mitad). Presiona otra vez para repetirla.")
                elif campo == "FIN":
                    completas += 1
                    print(f"Prueba completa ({completas} de {args.pruebas}).")
                    if completas < args.pruebas:
                        print("Presiona el boton azul para la siguiente prueba (PRBS9, unos 32 s).")
        except KeyboardInterrupt:
            print("\nCancelado.")
    print(f"Listo. {completas} prueba(s) completa(s) guardadas en {args.salida}")


if __name__ == "__main__":
    main()
