#pragma once
// Limite d'installation confirmée : certains paramètres max_current de la
// WB-01 peuvent annoncer une contrainte secondaire de 16 A non applicable.
static constexpr int WB_MANUAL_INSTALLATION_MAX_A = 32;
static constexpr int wb_manual_current_limit(int configured) {
  return configured < 6 || configured > 63 ? 0 :
    (configured < WB_MANUAL_INSTALLATION_MAX_A ? configured : WB_MANUAL_INSTALLATION_MAX_A);
}
static constexpr int wb_solar_current_limit(int configured) {
  return wb_manual_current_limit(configured);
}
