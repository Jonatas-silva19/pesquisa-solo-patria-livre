#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char* nomeRede = "Pesquisa solo -Patria livre";
const char* senhaRede = "Solo2026";

OneWire barramento(D2);
DallasTemperature sensores(&barramento);
ESP8266WebServer servidor(80);

const DeviceAddress sonda1 = {
  0x28, 0x5F, 0x4F, 0x49, 0xF6, 0x87, 0x3C, 0xE1
};

const DeviceAddress sonda2 = {
  0x28, 0x47, 0x17, 0x48, 0xF6, 0xFC, 0x3C, 0xAA
};

const unsigned long INTERVALO = 60UL * 60UL * 1000UL;
const size_t LIMITE = 60;

enum Recipiente : uint8_t {
  NAO_INFORMADO,
  COM_PALHA,
  EXPOSTO
};

struct Medicao {
  unsigned long numero;
  float temperatura1;
  float temperatura2;
  int umidadeBruta;
  Recipiente recipiente;
};

Medicao historico[LIMITE];
size_t proximaPosicao = 0;
size_t quantidade = 0;
unsigned long numeroMedicao = 0;
unsigned long ultimaColeta = 0;
Recipiente recipienteAtual = NAO_INFORMADO;

const char* nomeRecipiente(Recipiente recipiente) {
  switch (recipiente) {
    case COM_PALHA: return "Solo com palha";
    case EXPOSTO: return "Solo exposto";
    default: return "Não informado";
  }
}

String formatarTemperatura(float valor) {
  if (valor == DEVICE_DISCONNECTED_C) {
    return "Sonda sem resposta";
  }

  String texto = String(valor, 1);
  texto.replace('.', ',');
  return texto + " °C";
}

void registrarMedicao() {
  ultimaColeta = millis();
  sensores.requestTemperatures();

  Medicao& leitura = historico[proximaPosicao];
  leitura.numero = ++numeroMedicao;
  leitura.temperatura1 = sensores.getTempC(sonda1);
  leitura.temperatura2 = sensores.getTempC(sonda2);
  leitura.umidadeBruta = analogRead(A0);
  leitura.recipiente = recipienteAtual;

  Serial.printf(
    "Medicao %lu | Sonda 1: %s | Sonda 2: %s | Umidade bruta: %d | Garfo: %s\n",
    leitura.numero,
    formatarTemperatura(leitura.temperatura1).c_str(),
    formatarTemperatura(leitura.temperatura2).c_str(),
    leitura.umidadeBruta,
    nomeRecipiente(leitura.recipiente)
  );

  proximaPosicao = (proximaPosicao + 1) % LIMITE;
  if (quantidade < LIMITE) {
    quantidade++;
  }
}

void alterarRecipiente() {
  const String local = servidor.arg("local");

  if (local == "palha") {
    recipienteAtual = COM_PALHA;
  } else if (local == "exposto") {
    recipienteAtual = EXPOSTO;
  } else {
    servidor.send(400, "text/plain; charset=utf-8", "Selecione um recipiente.");
    return;
  }

  servidor.sendHeader("Location", "/");
  servidor.send(303, "text/plain", "");
}

void mostrarPagina() {
  const Medicao& ultima = historico[(proximaPosicao + LIMITE - 1) % LIMITE];

  servidor.sendHeader("Cache-Control", "no-store");
  servidor.setContentLength(CONTENT_LENGTH_UNKNOWN);
  servidor.send(200, "text/html; charset=utf-8", "");

  servidor.sendContent(
    "<!doctype html><html lang='pt-BR'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta http-equiv='refresh' content='60'>"
    "<title>Pesquisa de solo Pátria Livre/MST</title>"
    "<style>"
    "body{font:1rem system-ui,sans-serif;max-width:52rem;margin:2rem auto;"
    "padding:0 1rem;color:#243228;background:#f5f6f2}"
    "section{background:white;border:1px solid #d7dfd6;"
    "border-radius:.5rem;padding:1rem;margin:.75rem 0}"
    "button{padding:.55rem;margin:.25rem .4rem .25rem 0;font:inherit}"
    ".tabela{overflow-x:auto}table{border-collapse:collapse;width:100%}"
    "th,td{text-align:left;padding:.55rem;border-bottom:1px solid #d7dfd6}"
    "</style></head><body><main>"
    "<h1>Pesquisa de solo Pátria Livre/MST</h1>"
  );

  String resumo;
  resumo.reserve(750);
  resumo += F("<section><h2>Última medição</h2><p>Sonda 1: <strong>");
  resumo += formatarTemperatura(ultima.temperatura1);
  resumo += F("</strong></p><p>Sonda 2: <strong>");
  resumo += formatarTemperatura(ultima.temperatura2);
  resumo += F("</strong></p><p>Umidade bruta: <strong>");
  resumo += String(ultima.umidadeBruta);
  resumo += F("</strong></p><p>Garfo nesta medição: ");
  resumo += nomeRecipiente(ultima.recipiente);
  resumo += F("</p></section><section><h2>Local do garfo para a próxima coleta</h2><p>");
  resumo += nomeRecipiente(recipienteAtual);
  resumo += F(
    "</p><form method='post' action='/recipiente'>"
    "<button name='local' value='palha'>Solo com palha</button>"
    "<button name='local' value='exposto'>Solo exposto</button>"
    "</form></section>"
  );
  servidor.sendContent(resumo);

  servidor.sendContent(
    "<section><h2>Relatório de medições</h2>"
    "<p>Uma coleta ao ligar e uma por hora. As mais recentes aparecem primeiro.</p>"
    "<div class='tabela'><table><thead><tr>"
    "<th scope='col'>Medição</th><th scope='col'>Sonda 1</th>"
    "<th scope='col'>Sonda 2</th><th scope='col'>Umidade bruta</th>"
    "<th scope='col'>Local do garfo</th>"
    "</tr></thead><tbody>"
  );

  String linha;
  linha.reserve(200);
  for (size_t i = 0; i < quantidade; i++) {
    const size_t posicao = (proximaPosicao + LIMITE - 1 - i) % LIMITE;
    const Medicao& leitura = historico[posicao];

    linha = F("<tr><td>");
    linha += String(leitura.numero);
    linha += F("</td><td>");
    linha += formatarTemperatura(leitura.temperatura1);
    linha += F("</td><td>");
    linha += formatarTemperatura(leitura.temperatura2);
    linha += F("</td><td>");
    linha += String(leitura.umidadeBruta);
    linha += F("</td><td>");
    linha += nomeRecipiente(leitura.recipiente);
    linha += F("</td></tr>");
    servidor.sendContent(linha);
  }

  servidor.sendContent("</tbody></table></div></section></main></body></html>");
  servidor.sendContent("");
}

void setup() {
  Serial.begin(115200);
  sensores.begin();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(nomeRede, senhaRede, 1, false, 8)) {
    Serial.println("Falha ao iniciar a rede Wi-Fi.");
    return;
  }

  registrarMedicao();
  servidor.on("/", HTTP_GET, mostrarPagina);
  servidor.on("/recipiente", HTTP_POST, alterarRecipiente);
  servidor.begin();

  Serial.print("Endereco da pagina: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  if (quantidade > 0 && millis() - ultimaColeta >= INTERVALO) {
    registrarMedicao();
  }

  servidor.handleClient();
}

