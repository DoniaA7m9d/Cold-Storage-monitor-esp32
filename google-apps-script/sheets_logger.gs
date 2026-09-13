function doGet(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

  var temp  = e.parameter.temp;
  var hum   = e.parameter.hum;
  var alarm = e.parameter.alarm;

  var timestamp = new Date();

  sheet.appendRow([
    timestamp,
    temp,
    hum,
    alarm === "1" ? "ALARM" : "OK"
  ]);

  return ContentService
    .createTextOutput("OK")
    .setMimeType(ContentService.MimeType.TEXT);
}
