// SHROOM Flight Software

#ifndef FLIGHT_SOFTWARE_STORAGE_DOWNLOAD_H
#define FLIGHT_SOFTWARE_STORAGE_DOWNLOAD_H


/**
 * @brief Process USB-Serial commands and continue an active file transfer.
 *
 * Enter with DOWNLOAD,ENTER. Logging then remains stopped until reset.
 */
void storage_download_update();


/** @brief Check whether the dedicated download mode is active. */
bool storage_download_is_active();


#endif // FLIGHT_SOFTWARE_STORAGE_DOWNLOAD_H
