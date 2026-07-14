# libsdxi Event Handling Guide

The public event surface is context-scoped.

## Context Eventfds

Bind a context eventfd during context creation. There is no separate public post-create setter.

Use:

- `SDXI_CXT_CREATE_F_EVENTFD`
- `struct sdxi_cxt_create_opts.eventfd`

Query the bound state with:

- `sdxi_cxt_get_eventfd()`
- `sdxi_cxt_get_intr_info()`

If `eventfd == -1` and the create flag is set, the library allocates an eventfd and returns it through `sdxi_cxt_get_eventfd()`.

## Library Wait Behavior

High-level dataplane helpers in `sdxi/dp.h` use the bound eventfd when one is available. During normal helper waits, the library may poll and drain the eventfd internally.

Applications that need raw eventfd notifications should use the `sdxi/dp.h` raw mappings and their own synchronization. High-level helper waits do not preserve every eventfd counter edge.

## Recommended Pattern

For a dataplane helper application:

1. request a context eventfd during `sdxi_cxt_create()`
2. let the high-level helper path use it implicitly for waits
3. query the bound fd and interrupt metadata only when the application needs
  external integration or debugging
