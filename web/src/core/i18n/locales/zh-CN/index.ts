import type { LocaleDomainMessages } from '../../types'
import common from './common.json'
import app from './app.json'
import settings from './settings.json'
import gallery from './gallery.json'
import about from './about.json'
import onboarding from './onboarding.json'
import menu from './menu.json'
import extensions from './extensions.json'
import home from './home.json'
import map from './map.json'

export default [
  { domain: 'common', messages: common },
  { domain: 'app', messages: app },
  { domain: 'settings', messages: settings },
  { domain: 'gallery', messages: gallery },
  { domain: 'about', messages: about },
  { domain: 'onboarding', messages: onboarding },
  { domain: 'menu', messages: menu },
  { domain: 'extensions', messages: extensions },
  { domain: 'home', messages: home },
  { domain: 'map', messages: map },
] as const satisfies readonly LocaleDomainMessages[]
