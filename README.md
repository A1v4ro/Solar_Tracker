# Sistema Autónomo Portátil de Control de Seguimiento Solar

## 📌 Descripción

Este proyecto corresponde al desarrollo de un **sistema autónomo portátil de seguimiento solar** orientado a maximizar la captación de energía en módulos fotovoltaicos de baja potencia.

El sistema integra control basado en lógica difusa, registro de datos, supervisión web y alimentación autónoma en un diseño compacto y funcional.

Proyecto académico desarrollado como trabajo final de Ingeniería Electrónica.

---

## 🎯 Objetivo

Diseñar e implementar un sistema electrónico portátil capaz de optimizar la orientación de un panel fotovoltaico mediante control inteligente, incrementando la eficiencia de captación energética respecto a un sistema fijo.

---

## 🧠 Arquitectura del Sistema

El sistema está compuesto por:

- Sensor solar basado en LDRs
- Controlador difuso tipo Sugeno
- Control PWM para motores DC
- Puente H L298N con aislamiento
- Sistema de registro de datos (microSD + RTC DS3231)
- Módulo de alimentación autónoma con batería 18650
- Interfaz HMI basada en servidor web
- Prototipo mecánico diseñado en SolidWorks

---

## 📂 Estructura del Repositorio

```
/Codigo Seguidor Solar      → Firmware del microcontrolador (platformio)
/XFUZZY controlador         → Diseño del controlador difuso (Xfuzzy)
/Mecanismo Seguidor Solar   → Modelo mecánico del prototipo (SolidWorks)
/PCB KiCad                  → Diseño de la PCB (esquemático y layout)
```

---

# 🔧 Firmware – PlatformIO

## Requisitos

- Visual Studio Code
- Extensión PlatformIO instalada
- Placa configurada en `platformio.ini`

## Cómo abrir el proyecto

1. Abrir VS Code  
2. Seleccionar:  
   File → Open Folder  
3. Abrir la carpeta `/Codigo Seguidor Solar`

PlatformIO detectará automáticamente la configuración del proyecto.

## Compilar el firmware

```bash
pio run
```

## Cargar al microcontrolador

```bash
pio run --target upload
```

## Cargar a la memoria interna del microcontrolador

```bash
pio run --target uploadfs
```

---

# 🧠 Controlador Difuso – Xfuzzy

El controlador fue diseñado utilizando Xfuzzy.

## Abrir el proyecto

1. Ejecutar Xfuzzy  
2. Abrir el archivo `.xfl` ubicado en la carpeta `/XFUZZY controlador`

## Generar código C

Dentro de Xfuzzy:

Tools → Code Generation  
Seleccionar lenguaje C  
Generar archivo fuente

El archivo generado se integra dentro del firmware en PlatformIO.

---

# 🏗 Diseño Mecánico – SolidWorks

La carpeta `/Mecanismo Seguidor Solar` contiene:

- Archivos de pieza (.SLDPRT)
- Ensamblajes (.SLDASM)

## Uso

1. Abrir SolidWorks  
2. Abrir el archivo de ensamblaje principal  
3. Mantener todas las piezas dentro de la misma carpeta

No requiere configuración adicional.

---

# 🖥 Diseño Electrónico – KiCad

El sistema electrónico fue diseñado utilizando KiCad.

La carpeta `/PCB KiCad` contiene:

- Archivo de proyecto (.kicad_pro)
- Esquemático (.kicad_sch)
- Diseño de PCB (.kicad_pcb)
- Archivos de fabricación (`/GERBER`)

## Requisitos

- KiCad versión 9 o superior (recomendado)

## Cómo abrir el proyecto

1. Abrir KiCad
2. Seleccionar "Open Project"
3. Abrir el archivo `.kicad_pro` ubicado en la carpeta `/PCB KiCad`

---

# ⚙ Características Técnicas

- Control difuso tipo Sugeno
- Defuzzificación por promedio ponderado
- Control PWM para motores DC
- Registro de datos en microSD
- RTC DS3231 para referencia temporal
- Alimentación autónoma con batería 18650
- Supervisión mediante servidor web

---

# 📊 Funcionamiento General

1. El sensor solar detecta el desbalance de iluminación.
2. El controlador difuso evalúa las reglas establecidas.
3. Se calcula la señal PWM mediante promedio ponderado.
4. Se acciona el motor para corregir la orientación.
5. Se registran las variables del sistema.
6. Se permite la supervisión mediante interfaz web, operando en dos modos de red:

   - **Modo STA (Station Mode):**
     El sistema se conecta a una red WiFi existente.  
     El acceso a la interfaz se realiza desde un ordenador mediante mDNS utilizando:
     
     http://solartracker.local

   - **Modo AP (Access Point Mode):**
     El sistema genera su propia red WiFi.  
     El acceso a la interfaz se realiza directamente mediante la dirección IP:
     
     http://192.168.4.1

---

# 📌 Estado del Proyecto

Proyecto académico finalizado.  
Sistema funcional validado experimentalmente.

---

# 👤 Autor

Alvaro Jhonny Ortega Quispe  
Ingeniería Electrónica UMSA  
