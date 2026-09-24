// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

// Sagt ChattiFlexii, welchen Twitch-Kanal der Browser gerade zeigt. Mehr
// macht diese Erweiterung nicht: nichts wird in die Seite eingebaut, nichts
// wird gelesen außer der Adresse des Tabs, der gerade vorne ist.

const HOST = 'com.chatterino.chatterino';

// Seiten unter twitch.tv/…, die kein Kanal sind
const KEINE_KANAELE = new Set([
  'directory', 'downloads', 'drops', 'friends', 'inventory', 'jobs',
  'messages', 'moderator', 'p', 'payments', 'popout', 'prime', 'search',
  'settings', 'store', 'subscriptions', 'team', 'turbo', 'u', 'videos',
  'wallet',
]);

/// Der Kanalname in @a url, oder null, wenn die Seite kein Kanal ist
function kanalVon(url) {
  if (!url) {
    return null;
  }
  const treffer = /^https?:\/\/(?:www\.)?twitch\.tv\/([A-Za-z0-9_]+)\/?(?:[?#].*)?$/
    .exec(url);
  if (!treffer) {
    return null;
  }
  const name = treffer[1].toLowerCase();
  return KEINE_KANAELE.has(name) ? null : name;
}

// Was zuletzt gemeldet wurde, damit nicht bei jedem Ereignis dasselbe
// noch einmal geht
let zuletzt = null;

function melde(name, fensterId) {
  if (!name || name === zuletzt) {
    return;
  }
  zuletzt = name;

  // Eine Nachricht, ein kurzer Host-Prozess - ChattiFlexii nimmt sie über
  // seine Warteschlange entgegen. Eine dauerhafte Verbindung bringt nichts,
  // weil der Host sich nach zehn Sekunden ohne Nachricht selbst beendet.
  chrome.runtime.sendNativeMessage(
    HOST,
    {
      action: 'select',
      type: 'twitch',
      version: 0,
      winId: String(fensterId ?? 1),
      name,
    },
    () => {
      if (chrome.runtime.lastError) {
        // Beim nächsten Mal wieder versuchen
        zuletzt = null;
        console.warn(
          'ChattiFlexii nicht erreicht:',
          chrome.runtime.lastError.message,
        );
      }
    },
  );
}

/// Schaut nach, was im Fenster @a fensterId gerade vorne ist
async function schauNach(fensterId) {
  try {
    const abfrage = { active: true };
    if (fensterId === undefined || fensterId === null) {
      abfrage.lastFocusedWindow = true;
    } else {
      abfrage.windowId = fensterId;
    }

    const tabs = await chrome.tabs.query(abfrage);
    if (tabs.length === 0) {
      return;
    }
    const tab = tabs[0];
    melde(kanalVon(tab.url), tab.windowId);
  } catch (fehler) {
    console.warn('ChattiFlexii: konnte den Tab nicht lesen:', fehler);
  }
}

// Ein anderer Tab kommt nach vorne - das kann die fremde Erweiterung nicht
chrome.tabs.onActivated.addListener(info => {
  schauNach(info.windowId);
});

// In einem Tab wird ein anderer Kanal aufgerufen
chrome.tabs.onUpdated.addListener((tabId, aenderung, tab) => {
  if (!tab.active) {
    return;
  }
  if (aenderung.url || aenderung.status === 'complete') {
    melde(kanalVon(tab.url), tab.windowId);
  }
});

// Ein anderes Browserfenster wird ausgewählt
chrome.windows.onFocusChanged.addListener(fensterId => {
  if (fensterId === chrome.windows.WINDOW_ID_NONE) {
    return;
  }
  schauNach(fensterId);
});

// Und einmal, sobald der Browser oder die Erweiterung startet
chrome.runtime.onStartup.addListener(() => schauNach());
chrome.runtime.onInstalled.addListener(() => schauNach());
