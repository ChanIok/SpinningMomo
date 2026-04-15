import type { MapMarker, MapRenderOptions, MapRuntimeOptions } from '@/features/map/store'
import { buildMapRuntimeScriptFromPayload } from '@/features/map/injection/source'

type RuntimePayload = {
  markers: MapMarker[]
  renderOptions: MapRenderOptions
  runtimeOptions: MapRuntimeOptions
}

export function buildMapRuntimeScript(payload: RuntimePayload): string {
  const serializedPayload = JSON.stringify(payload).replace(/</g, '\\u003c')
  return buildMapRuntimeScriptFromPayload(serializedPayload)
}
