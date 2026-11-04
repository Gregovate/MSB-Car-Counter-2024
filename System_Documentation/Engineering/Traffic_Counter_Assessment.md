# MSB traffic counter engineering assessment

Status: REVIEW DRAFT. One combined assessment covers this Car Counter and the Gate Counter; it is maintained with the Gate project to avoid diverging copies.

- [Combined assessment review — Gate PR #2](https://github.com/Gregovate/MSB-Gate-Counter-2025/pull/2)
- [Assessment at reviewed commit](https://github.com/Gregovate/MSB-Gate-Counter-2025/blob/ce528a9a3ef516d7e5994191703df77b085c75e1/System_Documentation/Engineering/Traffic_Counter_Assessment.md)
- [MQTT and HA contract](https://github.com/Gregovate/MSB-Gate-Counter-2025/blob/ce528a9a3ef516d7e5994191703df77b085c75e1/System_Documentation/Engineering/MQTT_HA_Contract.md)
- [Refactoring and verification plan](https://github.com/Gregovate/MSB-Gate-Counter-2025/blob/ce528a9a3ef516d7e5994191703df77b085c75e1/System_Documentation/Engineering/Refactoring_Plan.md)
- [Operations/runbook gaps](https://github.com/Gregovate/MSB-Gate-Counter-2025/blob/ce528a9a3ef516d7e5994191703df77b085c75e1/System_Documentation/Engineering/Operations_Readiness.md)
- [Immutable source index](https://github.com/Gregovate/MSB-Gate-Counter-2025/blob/ce528a9a3ef516d7e5994191703df77b085c75e1/System_Documentation/Engineering/Source_Index.md)

Car baseline: firmware25.12.03.2 at release commit5a01aec573f2b08c6cc3f36995af3aedd42e56c9. Main928d6c61c4cfa0275239c8e168c13f84fb73c525 contains a later CSV header correction under the same version. Exact live binary provenance remains unverified.

Recommendation: preserve the Car detector's proven decision rules and arch behavior while extracting matching service boundaries in both applications. Resolve startup restoration, blocking input delays, checkpoint outcomes and MQTT/HA compatibility in separately reviewed stages. Do not copy the Gate algorithm over the Car detector just because both use dual beams.

This PR adds documentation only. It does not change source, firmware binaries, UI assets, HA configuration or production devices. Existing project rules continue to govern future production changes.
