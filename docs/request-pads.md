# Request Pads & Servicing

PFC ships faction **aircraft service pads** — buildable Conflict compositions where players request a
plane from the faction catalog, and where a landed plane can taxi in to be **repaired, salvaged and
refueled** (and rearmed, if the aircraft mod wires its armament system in — see
[Rearming](#rearming-armament-system-specific)). The flow is the vanilla helipad architecture: a service
provider with a request action, an entity-spawner slot, and support-station zones, all bundled into one
placeable composition.

## What ships in the core

| Prefab | Purpose |
|---|---|
| `Prefabs/Props/PFC_PlaneService_USSR.et` / `_US.et` | The service desk prop. Building provider + catalog spawner (filters on the vanilla `VEHICLE_AIRPLANE` label, 60 m building/slot radius) + the **Request Aircraft** action. |
| `Prefabs/Systems/PFC_PlaneSpawnerSlot.et` | 18×20 m `VEHICLE_LARGE` spawner slot for fighter/CAS-sized aircraft. |
| `Prefabs/Systems/PFC_PlaneSpawnerSlot_Heavy.et` | 34×36×13 m slot for transport-class aircraft (C-130 sized). |
| `Prefabs/Compositions/Slotted/SlotFlatLarge/PlaneService_L_USSR_01.et` / `_US_01.et` | The full pad: service prop + slot + repair/salvage zone + fuel point + runway lights + sign. |
| `PrefabsEditable/.../E_PlaneService_L_USSR_01.et` / `_US_01.et` | Editor wrappers — these are what appear in the GM editor and the Conflict building browser. |

The compositions are registered into the placeable-compositions registry and the building-mode browser by
same-GUID config overrides inside PFC, so simply loading the core makes the pads placeable.

!!! warning "The airplane browser tab"
    Vanilla has **no VEHICLE_AIRPLANE tab** in the request/building content browser — without one the
    request menu is empty even when the faction catalog has valid airplane entries, because browser tabs
    gate content by label. PFC's `Prefabs/Editor/Modes/EditorModeBuilding.et` override appends an
    airplane tab. Don't ship a second override of the same prefab in a consumer mod; if you must extend
    it, append with `+` semantics the way PFC does.

## Placing a pad

- **Game master**: both pads are under Systems → compositions in the editor content browser
  (faction-filtered).
- **Conflict**: the pads are registered in the free-roam-building lists (regular and HQ-commander),
  so they appear in the base building browser at the command post / building table alongside the
  vanilla helipads; the request action needs the base's supply storage in range for spawning costs.

Keep the slot area clear: the spawner slot reads **permanently occupied** if any static prop intersects
its oriented bounding box — that's why the shipped composition keeps the runway lights just outside the
slot corners. If your pad refuses to spawn planes, look for scenery inside the slot volume first.

## Making your plane requestable

Everything below happens in *your* aircraft mod. The chain the engine walks is:
prefab label → faction catalog entry → spawner label filter → browser tab → request action. Break any
link and the plane silently never shows up.

1. **Label the prefab.** In your plane prefab's `SCR_EditableVehicleComponent` UIInfo, set the authored
   label `VEHICLE_AIRPLANE` (not `VEHICLE_HELICOPTER`).
2. **Add a faction catalog entry** with a same-GUID override of the faction's vehicle catalog config,
   appending a multilist (`m_aMultiLists +` — a delta merge, never a full copy):

    ```text
    // Configs/EntityCatalog/USSR/Vehicles_EntityCatalog_USSR.conf  (.meta reuses the vanilla GUID)
    SCR_EntityCatalogMultiList {
     m_aMultiLists + {
      SCR_EntityCatalogMultiListEntry "{<new guid>}" {
       m_sIdentifier "MyPlane"
       m_aEntities {
        SCR_EntityCatalogEntry "{<new guid>}" {
         m_sEntityPrefab "{<guid>}Prefabs/Vehicles/MyPlane.et"
         m_aEntityDataList {
          SCR_EntityCatalogEditorData "{<new guid>}" {
           m_eValidEditorModes 33
          }
          SCR_EntityCatalogSpawnerData "{<new guid>}" {
           m_eSlotTypes VEHICLE_LARGE
           m_iSupplyCost 900
          }
         }
        }
       }
      }
     }
    }
    ```

3. **Match the slot type.** `m_eSlotTypes VEHICLE_LARGE` matches both PFC slots. There is currently no
   dedicated heavy slot type — a C-130-class entry will also list on the small pad, so keep spawn
   footprints in mind when choosing supply costs and pad placement.

That's it — the PFC service desk's spawner filters the faction vehicle catalog down to
`VEHICLE_AIRPLANE`-labeled entries and lists them in the request browser.

## Servicing on the pad

A plane that lands and taxis onto the pad is covered by the composition's support-station zones:

- **Repair & salvage** — the vanilla helipad repair station (bounding-box ranged, so the whole airframe
  counts, not just a point) plus vehicle salvage. Uses base supplies via its own resource consumer.
- **Refuel** — a faction fuel point at the pad edge. Its fuel support station is set to bounding-box
  range covering the slot, so the vanilla *Refuel* vehicle action works anywhere on the pad. The vehicle
  side is free: every `Vehicle_Base` descendant already carries `SCR_RefuelAtSupportStationAction`; you
  only need a `fuel_cap` action context on your prefab if you want the prompt anchored somewhere sensible.

### Rearming (armament-system specific)

Rearm is deliberately **not** in PFC — it depends on which armament system your aircraft uses, and the
core takes no weapon-system dependency. The pattern (exactly how WCS Armaments retrofits vanilla
helipads): your aircraft mod ships a **same-GUID override of the PFC composition** that appends the
armament system's resupply station and a supply consumer to the composition root. For WCS Armaments:

```text
// Prefabs/Compositions/Slotted/SlotFlatLarge/PlaneService_L_USSR_01.et
// (.meta reuses PFC's GUID {69A8A34000008201}; the file is a DELTA — PFC's content stays)
GenericEntity : "{6E9190BC2713551D}Prefabs/Compositions/Slotted/BuildableComposition_Base.et" {
 ID "5282DCB5DFC8538F"
 components {
  SCR_ResourceComponent "{<new guid>}" {
   m_aConsumers {
    SCR_ResourceConsumerServicePoint "{<new guid>}" {
     m_eResourceRights ALL
     m_fResourceRange 150
     // container queue / storage policy as in the WCS helipad overrides
    }
   }
  }
  WCS_Armament_RearmSupportStationComponent "{<new guid>}" {
   m_eSupportStationPriority HIGH
   m_fRange 20
  }
 }
}
```

The per-weapon rearm actions live on the pod/gun prefabs themselves (WCS ships them there), so with the
station in place the pilot gets rearm prompts on each mounted weapon while parked on the pad.

!!! note "Override etiquette"
    Same-GUID prefab overrides are deltas: list only what you add or change, keep the root `ID`, and
    never re-declare inherited components with fresh instance GUIDs — a fresh GUID *adds a duplicate
    component* instead of overriding.
