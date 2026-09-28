// SquachWatch on the FREE-WILi 2: NimBLE's host event queue. SquachWatch
// posts scan stops and restarts to it so they run on the Bluetooth host
// task; there is no second task here, so a posted event runs at once.
#pragma once
struct ble_npl_event;
typedef void ble_npl_event_fn(struct ble_npl_event*);
struct ble_npl_event { ble_npl_event_fn* fn; void* arg; };
struct ble_npl_eventq { int unused; };
inline void ble_npl_event_init(struct ble_npl_event* ev, ble_npl_event_fn* fn, void* arg) { ev->fn = fn; ev->arg = arg; }
inline struct ble_npl_eventq* nimble_port_get_dflt_eventq() { static ble_npl_eventq q; return &q; }
inline void ble_npl_eventq_put(struct ble_npl_eventq*, struct ble_npl_event* ev) { if (ev && ev->fn) ev->fn(ev); }
