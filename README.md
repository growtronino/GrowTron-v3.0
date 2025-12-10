<div align="center">

# 🌱 GROWTRON

### Sistema Inteligente de Automação de Cultivo Indoor

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
[![ESP32](https://img.shields.io/badge/ESP32-DevKit_V1-blue.svg)](https://www.espressif.com/)
[![Arduino](https://img.shields.io/badge/Framework-Arduino-00979D.svg)](https://www.arduino.cc/)
[![Version](https://img.shields.io/badge/Version-3.0-brightgreen.svg)]()
[![Status](https://img.shields.io/badge/Status-Production_Ready-success.svg)]()
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](http://makeapullrequest.com)

<p align="center">
  <img src="docs/images/growtron-banner.png" alt="Growtron Banner" width="600">
</p>

**Controle total do seu cultivo indoor com irrigação inteligente, iluminação com Efeito Emerson e monitoramento em tempo real.**

[Funcionalidades](#-funcionalidades) •
[Instalação](#-instalação) •
[Hardware](#-hardware) •
[Documentação](#-documentação) •
[Contribuir](#-como-contribuir)

</div>

---

## 📋 Sobre o Projeto

O **Growtron** é um sistema embarcado completo de automação para cultivo indoor, desenvolvido para ESP32. Ele oferece controle preciso de irrigação, gerenciamento inteligente de iluminação com suporte ao **Efeito Emerson**, monitoramento ambiental em tempo real e integração com serviços de nuvem.

### 🎯 Problema que Resolve

Cultivadores indoor enfrentam desafios constantes com:
- **Irrigação inconsistente** — plantas recebendo água demais ou de menos
- **Fotoperíodo impreciso** — luzes ligando/desligando em horários errados
- **Falta de monitoramento** — não saber as condições reais do ambiente
- **Ausência prolongada** — impossibilidade de viajar sem comprometer o cultivo

O Growtron automatiza todas essas tarefas, permitindo monitoramento e controle remoto via interface web responsiva.

### ✨ Diferenciais

| Característica | Benefício |
|----------------|-----------|
| **Efeito Emerson** | Maximiza a fotossíntese com LEDs far-red antes/depois do fotoperíodo |
| **Rega Pulsativa** | Previne compactação do solo e encharcamento |
| **Multi-Grow** | Suporta até 2 partições independentes com 4 vasos |
| **Zero Dependências Cloud** | Funciona 100% offline, integração com nuvem é opcional |
| **OTA Updates** | Atualize o firmware sem cabos |

---

## 🚀 Funcionalidades

### 🌡️ Monitoramento Ambiental
- Temperatura e umidade do ar (DHT11/DHT22)
- Luminosidade ambiente (sensor LDR)
- Umidade do solo por vaso (sensores capacitivos)
- Sincronização de horário via NTP

### 💧 Sistema de Irrigação
- **4 zonas independentes** (uma bomba/válvula por vaso)
- **Modo automático** com 2 horários programáveis por vaso
- **Modo pulsativo** configurável (pulsos on/off)
- **Modo contínuo** com tempo definido
- **Limites de umidade** para rega inteligente
- Calibração individual por sensor

### 💡 Controle de Iluminação
- **2 grows independentes** (Grow A e Grow B)
- **4 canais de luz** (Normal + Emerson por grow)
- **Efeito Emerson** com deslocamento configurável
- Suporte a ciclos que cruzam meia-noite
- Fotoperíodo totalmente personalizável

### 🖥️ Interface Web
- **Dashboard responsivo** com tema escuro moderno
- **Sistema de login** com sessões seguras
- **Configuração completa** via navegador
- **Visualização de câmera IP** integrada
- **Logs do sistema** persistentes
- **Página de ajuda** detalhada

### ☁️ Integrações
- **ThingSpeak** — envio de dados para nuvem (8 campos)
- **Câmera ESP32-CAM** — streaming de imagem no dashboard
- **OTA Updates** — atualização de firmware via web
- **API REST** — endpoints JSON para automação externa

### 📟 Display OLED
- Ciclo automático de 6 telas informativas
- Status de WiFi, IP, bombas, luzes
- Leituras de sensores em tempo real
- Hora NTP sincronizada

---

## 🔧 Hardware

### Componentes Necessários

| Componente | Quantidade | Especificação |
|------------|------------|---------------|
| ESP32 DevKit V1 | 1 | Dual-core, 4MB Flash |
| Display OLED | 1 | 0.91" ou 0.96" I2C 128x32 |
| Sensor DHT | 1 | DHT11 ou DHT22 |
| Sensor de Solo Capacitivo | 4 | Capacitive Soil Moisture v1.2 |
| Sensor LDR | 1 | Módulo ou resistor divisor |
| Módulo Relé | 8 | 5V, optoacoplado (4 luz + 4 rega) |
| Bomba/Válvula | 4 | 5V ou 12V (conforme projeto) |
| Fonte de Alimentação | 1 | 5V 3A (mínimo) |

### 📌 Diagrama de Conexões
