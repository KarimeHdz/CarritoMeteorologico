#include <Ethernet.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <DHT.h>

// Configuración DHT11
#define DHTPIN 3
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Configuración Ethernet y dirección de Base de Datos
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetClient clienteWeb;
IPAddress serverDB(129, 80, 212, 103); // IP del servidor MySQL
EthernetServer serverWeb(80);

char usuario[] = "sistemas";
char pass[] = "12345678";
char db_name[] = "carro";

MySQL_Connection conn((Client *)&clienteWeb);

unsigned long lastMillis = 0; // Para el control de tiempo de inserción

// Configuración de motores
int motorA1 = 8;
int motorA2 = 9;
int motorB1 = 11;
int motorB2 = 12;

float temperaturaActual = 0.0;

void setup() {
  Serial.begin(9600);
  dht.begin(); // Inicializa el sensor DHT11

  // Intentar obtener IP mediante DHCP repetidamente
  int max_reintentos = 10; // Número máximo de intentos
  int intentos = 0;
  while (Ethernet.begin(mac) == 0) {
    Serial.println("Fallo al obtener IP mediante DHCP. Reintentando...");
    intentos++;
    if (intentos >= max_reintentos) {
      Serial.println("Máximo de intentos alcanzado. Verifique su conexión.");
      while (true); // Detener si no se logra obtener una IP tras varios intentos
    }
    delay(5000); // Esperar 5 segundos antes de intentar nuevamente
  }

  serverWeb.begin(); // Inicia el servidor para el control del carro

  Serial.print("IP asignada: ");
  Serial.println(Ethernet.localIP());

  // Configuración de pines para motores
  pinMode(motorA1, OUTPUT);
  pinMode(motorA2, OUTPUT);
  pinMode(motorB1, OUTPUT);
  pinMode(motorB2, OUTPUT);
  detenerMotores();
}

void loop() {
  // Control de movimiento del carro
  atenderClienteWeb();

  // Control de temperatura y subida de datos
  if (millis() - lastMillis >= 30000 || millis() < lastMillis) { // Envía los datos cada 30 segundos
    lastMillis = millis();

    // Lee la temperatura del DHT11
    float temperatura = dht.readTemperature();

    if (isnan(temperatura)) {
      Serial.println("Error al leer el DHT11.");
    } else {
      temperaturaActual = temperatura; // Actualiza la temperatura
      subirDatosBD(temperatura); // Enviar datos a la base de datos
    }
  }
}

void atenderClienteWeb() {
  EthernetClient cliente = serverWeb.available();
  if (cliente) {
    boolean currentLineIsBlank = true;
    String request = "";

    while (cliente.connected()) {
      if (cliente.available()) {
        char c = cliente.read();
        request.concat(c);

        int posicion = request.indexOf("GET /");
        if (posicion != -1) {
          String comando = request.substring(posicion + 5, request.indexOf(" ", posicion + 5));

          // Control de motores basado en comando
          if (comando == "A") {
            moverDerecha();
          } else if (comando == "R") {
            moverIzquierda();
          } else if (comando == "D") {
            moverDerechaIzquierda();
          } else if (comando == "I") {
            moverIzquierdaDerecha();
          } else if (comando == "N") {
            detenerMotores();
          } else if (comando == "getTemperature") {
            enviarTemperatura(cliente); // Enviar la temperatura actual en formato JSON
            return;
          }
        }

        if (c == '\n' && currentLineIsBlank) {
          cliente.println("HTTP/1.1 200 OK");
          cliente.println("Content-Type: text/html");
          cliente.println();
          cliente.println("<!DOCTYPE html>");
          cliente.println("<html>");
          cliente.println("<head>");
          cliente.println("<title>Control de Carro</title>");
          cliente.println("<style>");
          cliente.println("body { font-family: Arial, sans-serif; background-color: #f0f0f5; margin: 0; padding: 0; text-align: center; }");
          cliente.println("h1 { color: #333; margin: 20px 0; }");
          cliente.println("button { background-color: #4CAF50; color: white; border: none; padding: 15px 30px; margin: 10px; font-size: 18px; border-radius: 8px; cursor: pointer; }");
          cliente.println("button:hover { background-color: #45a049; }");
          cliente.println("button:active { transform: scale(0.95); }");
          cliente.println(".container { max-width: 600px; margin: auto; padding: 20px; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }");
          cliente.println(".temperature { margin-top: 20px; font-size: 24px; color: #555; }");
          cliente.println("</style>");
          cliente.println("<script>");
          cliente.println("function enviarComando(comando) {");
          cliente.println("  var xhr = new XMLHttpRequest();");
          cliente.println("  xhr.open('GET', '/' + comando, true);");
          cliente.println("  xhr.send();");
          cliente.println("}");
          cliente.println("function obtenerTemperatura() {");
          cliente.println("  var xhr = new XMLHttpRequest();");
          cliente.println("  xhr.open('GET', '/getTemperature', true);");
          cliente.println("  xhr.onreadystatechange = function() {");
          cliente.println("    if (xhr.readyState == 4 && xhr.status == 200) {");
          cliente.println("      var respuesta = JSON.parse(xhr.responseText);");
          cliente.println("      document.getElementById('temperatura').innerText = 'Temperatura: ' + respuesta.temperatura + '°C';");
          cliente.println("    }");
          cliente.println("  };");
          cliente.println("  xhr.send();");
          cliente.println("}");
          cliente.println("setInterval(obtenerTemperatura, 10000);"); // Actualizar cada 10 segundos
          cliente.println("</script>");
          cliente.println("</head>");
          cliente.println("<body>");
          cliente.println("<div class='container'>");
          cliente.println("<h1>Control de Carro</h1>");
          cliente.println("<div>");
          cliente.println("<button onmousedown=\"enviarComando('A')\" onmouseup=\"enviarComando('N')\">Adelante</button><br>");
          cliente.println("<button onmousedown=\"enviarComando('I')\" onmouseup=\"enviarComando('N')\">Izquierda</button>");
          cliente.println("<button onmousedown=\"enviarComando('D')\" onmouseup=\"enviarComando('N')\">Derecha</button><br>");
          cliente.println("<button onmousedown=\"enviarComando('R')\" onmouseup=\"enviarComando('N')\">Atrás</button>");
          cliente.println("</div>");
          cliente.println("<div class='temperature' id='temperatura'>Temperatura: " + String(temperaturaActual) + "C</div>");
          cliente.println("</div>");
          cliente.println("</body>");
          cliente.println("</html>");
          break;
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    cliente.stop();
  }
}

void subirDatosBD(float temperatura) {
  // Conectar a la base de datos
  if (conn.connect(serverDB, 3306, usuario, pass)) {
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    char use_db[50];
    sprintf(use_db, "USE %s", db_name);
    cur_mem->execute(use_db);
    delete cur_mem;

    // Enviar datos
    char query[128];
    char tempStr[10];
    dtostrf(temperatura, 5, 2, tempStr);
    sprintf(query, "INSERT INTO carrotempo (lugar, temperatura, fecha_hora) VALUES ('Torreon', %s, NOW())", tempStr);

    cur_mem = new MySQL_Cursor(&conn);
    cur_mem->execute(query);
    delete cur_mem;

    conn.close();
  }
}

void detenerMotores() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, LOW);
}

void moverDerecha() {
  digitalWrite(motorA1, HIGH);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, HIGH);
  digitalWrite(motorB2, LOW);
}

void moverIzquierda() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, HIGH);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, HIGH);
}

void moverDerechaIzquierda() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, HIGH);
  digitalWrite(motorB1, HIGH);
  digitalWrite(motorB2, LOW);
}

void moverIzquierdaDerecha() {
  digitalWrite(motorA1, HIGH);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, HIGH);
}

void enviarTemperatura(EthernetClient &cliente) {
  cliente.println("HTTP/1.1 200 OK");
  cliente.println("Content-Type: application/json");
  cliente.println();
  cliente.println("{\"temperatura\":" + String(temperaturaActual) + "}");
}
