# Project Rules

This is the repository-level instruction portal for `MSB-Car-Counter-2024`.

When Greg says **read the project instructions**, read this README and every linked rule relevant to the requested work before proposing changes.

## Mandatory Production Gate

**Production mutation commands are forbidden until the governing runbook has been retrieved from the responsible repository and read in the current workstream.**

- [Runbook-First Production Rule](Runbook_First_Production_Rule.md) — mandatory for Production deployments, server/service changes, configuration changes, recovery actions, and other Production mutations. It requires identifying and reading the governing runbook before commands, stating the authority/procedure/current step, stopping after failures, and declaring `RUNBOOK GAP FOUND` instead of improvising when documentation is incomplete.

## Rule Ownership

Repository-specific engineering and operating rules belong in this folder. Current implementation details and runtime facts belong in the responsible engineering/runbook documentation rather than being duplicated here.