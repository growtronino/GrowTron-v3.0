# GrowTron-v3.0

Growtron 🌱 - Sistema de Automação de Cultivo Inteligente
PlatformIO
Framework
License
Status

Growtron é um firmware avançado para ESP32 projetado para automação de precisão em ambientes de cultivo indoor (Grows). Ele oferece controle granular sobre iluminação (incluindo Efeito Emerson), irrigação baseada em umidade do solo, monitoramento ambiental e integração com IoT, tudo gerenciado através de um Dashboard Web responsivo hospedado no próprio microcontrolador.

🎯 Visão Geral
O projeto resolve a necessidade de controladores de cultivo acessíveis, porém robustos, eliminando a dependência de timers mecânicos e aferições manuais. O Growtron atua como um hub central que:

Gerencia fotoperíodos complexos.
Automatiza a rega com base na necessidade real da planta (sensores capacitivos).
Fornece telemetria remota via Web e Cloud.
✨ Funcionalidades Principais
Controle de Iluminação Dual-Zone: Suporte para 2 partições (Grows) independentes com controle de luz principal e auxiliar (Efeito Emerson/Far-Red) com offsets programáveis.
Irrigação Inteligente: Controle para até 4 vasos com sensores capacitivos. Modos de rega contínua ou pulsativa (para melhor absorção do solo).
Dashboard Web Responsivo: Interface moderna, escura e mobile-friendly (sem dependências externas de CSS/JS) para monitoramento e configuração em tempo real.
Monitoramento Ambiental: Leitura de Temperatura, Umidade (DHT11/22) e Luminosidade (LDR).
Conectividade & IoT:
Integração nativa com ThingSpeak para datalogging na nuvem.
Visualização de Câmeras IP (ESP32-CAM) diretamente no dashboard.
Sincronização de horário NTP.
Robustez: Sistema de arquivos LittleFS para logs persistentes, atualizações OTA (Over-The-Air) e modo AP de Fallback para configuração de WiFi.
🛠️ Hardware e Arquitetura
O sistema foi projetado para o ESP32 DevKit V1. Abaixo está o mapa de pinagem (Pinout) padrão definido no firmware.

Componente	Função	GPIO (ESP32)	Notas
Sensores			
DHT11 / DHT22	Temp/Umidade	GPIO 4	Configurável na Web
LDR	Luminosidade	GPIO 32	Leitura Analógica
Capacitivo V1	Solo Vaso 1	GPIO 36 (VP)	ADC1 Apenas
Capacitivo V2	Solo Vaso 2	GPIO 39 (VN)	ADC1 Apenas
Capacitivo V3	Solo Vaso 3	GPIO 34	ADC1 Apenas
Capacitivo V4	Solo Vaso 4	GPIO 35	ADC1 Apenas
Atuadores			
Relé Luz A	Grow A - Normal	GPIO 23	Lógica Invertida (Low=On)
Relé Luz A (E)	Grow A - Emerson	GPIO 5	
Relé Luz B	Grow B - Normal	GPIO 18	
Relé Luz B (E)	Grow B - Emerson	GPIO 19	
Relé Rega 1-4	Bombas	25, 26, 27, 14	
Outros			
OLED Display	I2C SDA/SCL	GPIO 21, 22	128x32 ou 128x64
Botão Reset	Factory Reset	GPIO 0 (BOOT)	Segurar 5s para resetar
🚀 Pré-requisitos
Para compilar e enviar o projeto, você precisará de:

Hardware: ESP32, Módulo Relé 8 canais (ou 2x4), Display OLED SSD1306, Sensores.
IDE: Recomenda-se VS Code com extensão PlatformIO (para fácil gerenciamento de libs e LittleFS). Alternativamente, Arduino IDE configurada para ESP32.
Drivers: Drivers USB para Ponte CP210x ou CH340 (dependendo da sua placa).
📦 Instalação e Execução
1. Clonar o Repositório
Bash

git clone https://github.com/seu-usuario/growtron.git
cd growtron
2. Configurar Dependências
As seguintes bibliotecas são obrigatórias (instalação via Library Manager):

ESPAsyncWebServer & AsyncTCP
Adafruit SSD1306 & Adafruit GFX
DHT sensor library
NTPClient
ArduinoJson
3. Configuração da Partição (Crucial)
Como o projeto utiliza LittleFS para logs e OTA para atualizações, você deve selecionar um esquema de partição adequado na IDE.

Arduino IDE: Tools > Partition Scheme > Huge APP (3MB No OTA/1MB SPIFFS) (ou similar).
PlatformIO: Adicione board_build.partitions = huge_app.csv no platformio.ini.
4. Upload
Compile e faça o upload do código para o ESP32.

Nota: Certifique-se de que a imagem do sistema de arquivos LittleFS foi formatada/inicializada corretamente na primeira execução (o código lida com isso automaticamente).

⚙️ Configuração Inicial
Ao iniciar pela primeira vez, o ESP32 criará um Ponto de Acesso (Hotspot).
Conecte-se à rede WiFi: GrowtronAP | Senha: growtron123.
Abra o navegador e acesse: http://192.168.4.1.
Faça login com as credenciais padrão:
Usuário: admin
Senha: 1234
Navegue até a aba Config (Cfg) e configure o SSID e Senha da sua rede WiFi local. O sistema reiniciará e se conectará à sua rede.
🤝 Como Contribuir
Contribuições são bem-vindas! Se você tiver ideias para melhorar a lógica de irrigação ou novas integrações:

Faça um Fork do projeto.
Crie uma Branch para sua Feature (git checkout -b feature/NovaFeature).
Faça o Commit (git commit -m 'Adiciona suporte a sensor XYZ').
Faça o Push (git push origin feature/NovaFeature).
Abra um Pull Request.
📄 Licença
Este projeto está licenciado sob a Licença MIT - veja o arquivo LICENSE para detalhes.

<p align="center">Desenvolvido com ❤️ para a comunidade Maker e Cultivadores.</p>
