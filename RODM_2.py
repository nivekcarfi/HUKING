import os
import time
import subprocess
import requests  # <- Asegúrate de tener esta librería instalada
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import mysql.connector


CARPETA_IMAGENES = '/home/huk/RODM/uploads'
ESP32_IP = '192.168.28.84'  # Dirección IP del ESP32
ESP32_PORT = 80  # Puerto para el control del servo


class EventoImagenHandler(FileSystemEventHandler):
   def on_created(self, event):
       if event.is_directory:
           return
       if not event.src_path.lower().endswith(('.jpg', '.jpeg', '.png')):
           return


       print(f"📸 Nueva imagen: {event.src_path}")
       matricula = reconocer_matricula(event.src_path)
       if matricula:
           print(f"✅ Matrícula detectada: {matricula}")
           if verificar_reserva(matricula):
               print(f"🔓 Reserva activa para {matricula}. Abriendo barrera...")
               abrir_barrera()
           else:
               print("🚫 No hay reserva activa para esta matrícula.")
       else:
           print("⚠️ No se pudo reconocer la matrícula")


def reconocer_matricula(ruta_imagen):
   comando = [
       'sudo', 'docker', 'run', '--rm',
       '-v', f'{os.path.dirname(ruta_imagen)}:/mnt',
       'openalpr/openalpr', '-c', 'eu',
       f'/mnt/{os.path.basename(ruta_imagen)}'
   ]
   resultado = subprocess.run(comando, capture_output=True, text=True)
   for linea in resultado.stdout.splitlines():
       if linea.startswith('    - '):  # Primera línea de resultados
           return linea.split()[1]  # Devuelve la matrícula con mayor confianza
   return None


def verificar_reserva(matricula):
   try:
       db = mysql.connector.connect(
           host='localhost',
           user='huk',
           password='huk3',
           database='DB_huk'
       )
       cursor = db.cursor()
       cursor.execute("""
           SELECT r.id, r.data_entrada, r.data_sortida, r.estat
           FROM vehicles v
           JOIN reservas r ON v.id = r.vehicle_id
           WHERE v.matricula = %s AND NOW() BETWEEN r.data_entrada AND r.data_sortida
       """, (matricula,))
       resultado = cursor.fetchone()
       cursor.close()
       db.close()
       return resultado is not None  # Devuelve True si hay una reserva activa
   except Exception as e:
       print(f"❌ Error en la base de datos: {e}")
       return False


def abrir_barrera():
   try:
       response = requests.get(f"http://{ESP32_IP}:{ESP32_PORT}/abrir")
       if response.status_code == 200:
           print("🚦 Barrera abierta con éxito")
       else:
           print("⚠️ Error al abrir la barrera")
   except Exception as e:
       print(f"❌ Error de conexión con el ESP32: {e}")


def iniciar_monitor():
   print(f"🕵️  Monitorizando carpeta: {CARPETA_IMAGENES}")
   observer = Observer()
   observer.schedule(EventoImagenHandler(), CARPETA_IMAGENES, recursive=False)
   observer.start()
   try:
       while True:
           time.sleep(1)
   except KeyboardInterrupt:
       observer.stop()
   observer.join()


if __name__ == "__main__":
   iniciar_monitor()
