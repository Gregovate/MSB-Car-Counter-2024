#pragma once
// One-time migration of the exact inspected and backed-up legacy DATA image.
// No generic format/erase command is exposed. Both OTA partitions are protected.
void serviceJournalProvisioning();
bool journalProvisioningActive();
const char* journalProvisioningState();

void publishJournalProvisioningState();
