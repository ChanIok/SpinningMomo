import type { SyncRuntimePayload } from '@/features/map/bridge/protocol'
import { buildMapDevEvalScriptFromPayload } from '@/features/map/injection/source'

export function buildMapDevEvalScript(payload: SyncRuntimePayload): string {
  const serializedPayload = JSON.stringify(payload).replace(/</g, '\\u003c')
  return buildMapDevEvalScriptFromPayload(serializedPayload)
}
