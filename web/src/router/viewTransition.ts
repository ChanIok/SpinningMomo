import type { RouteLocationRaw, Router } from 'vue-router'

type MinimalViewTransition = {
  finished: Promise<void>
}

type DocumentWithViewTransition = Document & {
  startViewTransition?: (updateCallback: () => void | Promise<void>) => MinimalViewTransition
}

let transitionQueue: Promise<void> = Promise.resolve()

function enqueueTransition(task: () => Promise<void>) {
  const next = transitionQueue.then(task, task)
  transitionQueue = next.then(
    () => undefined,
    () => undefined
  )
  return next
}

export function pushWithViewTransition(router: Router, to: RouteLocationRaw) {
  return enqueueTransition(async () => {
    const target = router.resolve(to)
    const current = router.currentRoute.value
    if (target.fullPath === current.fullPath) {
      return
    }

    const doc =
      typeof document !== 'undefined' ? (document as DocumentWithViewTransition) : undefined
    if (!doc?.startViewTransition) {
      await router.push(to)
      return
    }

    const transition = doc.startViewTransition(async () => {
      await router.push(to)
    })

    await transition.finished.catch(() => undefined)
  })
}
