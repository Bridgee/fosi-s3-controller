// Browser-only help. Reads visible UI state; never changes simulated controls.
(() => {
  'use strict';
  const page = (title, summary, actions, navigation) => ({title, summary, actions, navigation});
  const guides = {
    player: page('Player', 'Music and the main S3 controls.', [
      'Tap the volume value or arc to open Volume; drag the arc to adjust.',
      'Tap I/O for inputs, outputs and settings. Tap the speaker to mute.',
      'Tap artwork to enlarge it, or the lyrics bubble when it appears.',
      'Tap the title or artist for Artist, Release and Song introductions.'
    ], 'Swipe down: Clock · up: Weather · right: AirPlay · left: Reserved. Start away from the volume arc to change pages.'),
    volume: page('Volume', 'Adjust level and mute.', [
      'Tap − or + for one step, or drag the outer arc.',
      'Tap Mute / Unmute to toggle sound.',
      'A configured volume limit stops further increases.'
    ], 'Tap Back to return to the player.'),
    routing: page('Input / Output', 'Select an input and output.', [
      'Tap Bluetooth, Line In, HDMI or Optical to select an input.',
      'Choose Analog or Optical output.',
      'The top power icon enters standby. About is on the left; Theme is on the right.'
    ], 'Tap Back to return to the player. Network sources appear when a stream starts.'),
    theme: page('Theme & brightness', 'Change the display appearance.', [
      'Tap one of eight color swatches.',
      'Use − and + to adjust brightness.',
      'The browser remembers these settings on this site.'
    ], 'Tap Back to return to I/O.'),
    about: page('About', 'Connection details and firmware information.', [
      'Read the sample screen / S3 addresses and source status.',
      'These values are simulated; no device is connected.'
    ], 'Tap Back to return to the page you came from.'),
    'blank-up': page('Clock', 'Local time, alarm and countdown.', [
      'Select A1–A4 or T1–T4, then tap its time to edit.',
      'Use ON / OFF for an alarm, or RST to reset, STRT to start / resume, and PAU to pause for the selected timer.',
      'After 30 seconds, the clock dims and keeps time. Tap once to restore brightness.'
    ], 'Swipe up to return to the player.'),
    'clock-alarm': page('Alarm', 'Set a visual alarm.', [
      'Select HOUR or MINUTE, then use − and +.',
      'Tap SAVE to keep this alarm time. Enable it with ON / OFF on the clock.',
      'When due, the display lights up; the reminder pulses until STOP.'
    ], 'Tap BACK for the clock, or swipe up for the player.'),
    'clock-timer': page('Timer', 'Edit one of four independent countdowns.', [
      'Select MINUTE or SECOND, then use − and +.',
      'Tap SAVE, then use STRT / PAU and RST on the clock.',
      'Pause a running timer before editing. At zero, it lights the display.'
    ], 'Tap BACK for the clock, or swipe up for the player.'),
    'clock-alert': page('Time is up', 'An alarm or countdown needs attention.', [
      'The reminder pulses; tap the steady STOP button to dismiss it.',
      'The alert stays visible until you stop it; it makes no sound.'
    ], 'Dismiss the alert before navigating.'),
    'blank-down': page('Weather', 'Sample local conditions.', [
      'Read the condition, temperature, feels-like value and daily range.',
      'The lower row shows wind, humidity and precipitation probability.',
      'Location and weather are local samples, not live requests.'
    ], 'Swipe down to return to the player.'),
    'blank-left': page('AirPlay launch', 'Preview starting music from an iPhone.', [
      'Tap Play on S3 to see the sending / confirmation flow.',
      'If the S3 is in standby, the preview wakes it first.',
      'No push notification is sent and no phone is controlled.'
    ], 'Swipe left to return to the player.'),
    'blank-right': page('Reserved', 'A place for a future feature.', [
      'This page currently shows the direction of your swipe.'
    ], 'Swipe right to return to the player.'),
    intro: page('Music introductions', 'Choose a short reading about the music.', [
      'Tap Artist, Release or Song around the circle.',
      'Long titles and artist names scroll automatically.'
    ], 'Tap Back, or swipe right, to return to the player.'),
    'intro-read': page('Reading', 'A short introduction with source references.', [
      'Drag up or down inside the text to scroll.',
      'Sources appear at the end. Tap a reference for its link / QR view.',
      'Returning reuses this track’s completed sample. A failed sample offers Retry.'
    ], 'Tap Back, or swipe right, to choose another topic. Loading is simulated; no AI request is sent.'),
    'intro-sources': page('Source', 'A reference for the current introduction.', [
      'Read or scan the reference link; tap a source number to switch.',
      'Opening a link leaves the simulator only when you choose it.'
    ], 'Tap Back, or swipe right, to return to the reading.'),
    lyrics: page('Lyrics', 'Timed lines follow the sample track.', [
      'The current line is emphasized; surrounding lines are dimmed.',
      'Playback, pause and seeking drive the local lyric timeline.',
      'The lyrics page stays awake while it is open.'
    ], 'Tap the display to return to the player.'),
    artwork: page('Artwork', 'A larger view of the current cover.', [
      'Playback continues while the cover is enlarged.',
      'Tap the image or background to restore the player.'
    ], 'Tap once to close, or press Escape.'),
    standby: page('Standby', 'The S3 is asleep; neighboring pages still work.', [
      'Tap the central power icon to wake the simulated S3.',
      'AirPlay launch wakes the S3 before starting its simulated flow.'
    ], 'Swipe down: Clock · up: Weather · right: AirPlay · left: Reserved. Swiping does not wake the S3.'),
    dimmed: page('Clock · Dimmed', 'An always-visible clock at reduced brightness.', ['Time keeps updating while the display is dimmed.','Tap once to restore your brightness. This first touch does not change a control.'], 'After waking, swipe up to return to the player.'),
    asleep: page('Display asleep', 'The preview dims after 30 seconds without interaction.', [
      'Tap the round display once to wake it.',
      'That first tap only wakes the display; it does not activate a control.'
    ], 'Once lit, use the normal controls or swipe between pages.')
  };
  function selectGuide({view, asleep, standby, artwork, vinyl, dimmed}) {
    if (asleep) return {key:'asleep', ...guides.asleep};
    if (view === 'blank-up' && dimmed) return {key:'dimmed', ...guides.dimmed};
    if (view === 'player' && standby) return {key:'standby', ...guides.standby};
    if (view === 'player' && artwork) return {key:'artwork', ...guides.artwork};
    const key = Object.hasOwn(guides, view) ? view : 'player';
    if (key === 'player' && vinyl) return {key:'vinyl', ...guides.player, summary:'No artwork: a record and tonearm follow playback.', actions:[
      ...guides.player.actions.slice(0,2),
      'The highlights rotate during playback; the tonearm lifts when paused.',
      'Tap the title or artist for Artist, Release and Song introductions.'
    ]};
    return {key, ...guides[key]};
  }
  if (typeof module === 'object' && module.exports) module.exports = {guides, selectGuide};
  if (typeof document === 'undefined') return;
  const screen = document.getElementById('screen');
  if (!screen || document.getElementById('simulatorGuide')) return;
  const stage = screen.closest('.stage');
  const frame = screen.closest('.device-holder') || screen.closest('.device-wrap');
  if (!stage || !frame) return;
  const make = (tag, className, value) => {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (value) node.textContent = value;
    return node;
  };
  const shell = make('div','simulator-shell');
  const device = make('div','simulator-device');
  frame.before(shell); device.append(frame); shell.append(device);
  const aside = make('aside','simulator-guide');
  aside.id = 'simulatorGuide'; aside.setAttribute('aria-label','Simulator page guide');
  aside.append(make('p','simulator-guide-label','SIMULATOR GUIDE'));
  const content = make('div','simulator-guide-content');
  content.setAttribute('aria-live','polite');content.setAttribute('aria-atomic','true');
  aside.append(content,make('p','simulator-guide-pointer','Mouse / trackpad: press, drag, release.'));
  shell.append(aside);
  document.documentElement.classList.add('simulator-guided');
  let lastKey = '', queued = false;
  function update() {
    queued = false;
    const visible = screen.querySelector('.page:not([hidden])');
    const vinyl = document.getElementById('vinyl');
    const guide = selectGuide({view:visible?.id.replace('page-',''),
      asleep:screen.classList.contains('display-asleep'),dimmed:screen.classList.contains('display-dimmed'),standby:screen.classList.contains('standby'),
      artwork:screen.classList.contains('artwork-open'),vinyl:vinyl && !vinyl.hidden});
    if (guide.key === lastKey) return;
    lastKey = guide.key;aside.dataset.page = guide.key;
    const title = make('h2','',guide.title);title.id = 'simulatorGuideTitle';
    const summary = make('p','simulator-guide-summary',guide.summary);
    const list = make('ul','');guide.actions.forEach(value=>list.append(make('li','',value)));
    content.replaceChildren(title,summary,make('h3','','Try'),list,make('h3','','Navigation'),make('p','',guide.navigation));
  }
  function schedule() { if (!queued) {queued = true;requestAnimationFrame(update);} }
  function fit() {
    const width = Math.max(1,stage.clientWidth || window.innerWidth-48);
    const stacked = width < 900;
    const diameter = Math.min(528,stacked ? width : width-348);
    shell.classList.toggle('is-stacked',stacked);
    shell.style.setProperty('--simulator-scale',String(diameter/528));
    shell.style.setProperty('--simulator-diameter',diameter+'px');
  }
  const observer = new MutationObserver(schedule);
  observer.observe(screen,{attributes:true,attributeFilter:['class']});
  screen.addEventListener('orbit-view-change',schedule);
  new ResizeObserver(fit).observe(stage);
  window.addEventListener('resize',fit);
  fit();update();
})();
