#pragma once
// Read-only internal-flash inspection. The only writes are a verified SD backup;
// this service exposes NO internal-flash erase, format or initialize operation.
void setupJournalInspection();
void serviceJournalInspection();
