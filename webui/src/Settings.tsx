import { createSignal, onMount } from 'solid-js'

function Settings() {
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [elegooip, setElegooip] = createSignal('')
  const [startPrintTimeout, setStartPrintTimeout] = createSignal(10000)
  const [expectedDeficit, setExpectedDeficit] = createSignal(8.4)
  const [expectedWindow, setExpectedWindow] = createSignal(1500)
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [saveSuccess, setSaveSuccess] = createSignal(false)
  const [apMode, setApMode] = createSignal<boolean | null>(null);
  const [pauseOnRunout, setPauseOnRunout] = createSignal(true);
  const [enabled, setEnabled] = createSignal(true);
  const [sdcpLossBehavior, setSdcpLossBehavior] = createSignal(2);
  const [devMode, setDevMode] = createSignal(false);
  const [verboseLogging, setVerboseLogging] = createSignal(false);
  const [keepExpectedForever, setKeepExpectedForever] = createSignal(false);
  const [flowSummaryLogging, setFlowSummaryLogging] = createSignal(false);
  const [discovering, setDiscovering] = createSignal(false);
  const [discoverSuccess, setDiscoverSuccess] = createSignal(false);
  // Load settings from the server and scan for WiFi networks
  onMount(async () => {
    try {
      setLoading(true)

      // Load settings
      const response = await fetch('/get_settings')
      if (!response.ok) {
        throw new Error(`Failed to load settings: ${response.status} ${response.statusText}`)
      }
      const settings = await response.json()

      setSsid(settings.ssid || '')
      // Password won't be loaded from server for security
      setPassword('')
      setElegooip(settings.elegooip || '')
      setStartPrintTimeout(settings.start_print_timeout || 10000)
      setApMode(settings.ap_mode || null)
      setPauseOnRunout(settings.pause_on_runout !== undefined ? settings.pause_on_runout : true)
      setEnabled(settings.enabled !== undefined ? settings.enabled : true)
      setExpectedDeficit(settings.expected_deficit_mm !== undefined ? settings.expected_deficit_mm : 8.4)
      setExpectedWindow(settings.expected_flow_window_ms !== undefined ? settings.expected_flow_window_ms : 1500)
      setSdcpLossBehavior(settings.sdcp_loss_behavior !== undefined ? settings.sdcp_loss_behavior : 2)
      setDevMode(settings.dev_mode !== undefined ? settings.dev_mode : false)
      setVerboseLogging(settings.verbose_logging !== undefined ? settings.verbose_logging : false)
      setKeepExpectedForever(settings.keep_expected_forever !== undefined ? settings.keep_expected_forever : false)
      setFlowSummaryLogging(settings.flow_summary_logging !== undefined ? settings.flow_summary_logging : false)

      setError('')
    } catch (err: any) {
      setError(`Error loading settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to load settings:', err)
    } finally {
      setLoading(false)
    }
  })


  const handleSave = async () => {
    try {
      setSaveSuccess(false)
      setError('')

      const settings = {
        ssid: ssid(),
        passwd: password(),
        ap_mode: false,
        elegooip: elegooip(),
        pause_on_runout: pauseOnRunout(),
        start_print_timeout: startPrintTimeout(),
        enabled: enabled(),
        expected_deficit_mm: expectedDeficit(),
        expected_flow_window_ms: expectedWindow(),
        sdcp_loss_behavior: sdcpLossBehavior(),
        dev_mode: devMode(),
        verbose_logging: verboseLogging(),
        keep_expected_forever: keepExpectedForever(),
        flow_summary_logging: flowSummaryLogging(),
      }

      const response = await fetch('/update_settings', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
      })

      if (!response.ok) {
        throw new Error(`Failed to save settings: ${response.status} ${response.statusText}`)
      }

      setSaveSuccess(true)
      setTimeout(() => setSaveSuccess(false), 3000)
    } catch (err: any) {
      setError(`Error saving settings: ${err.message || 'Unknown error'}`)
      console.error('Failed to save settings:', err)
    }
  }
  const handleDiscover = async () => {
    try {
      setDiscoverSuccess(false)
      setError('')
      setDiscovering(true)

      const response = await fetch('/discover_printer')
      if (!response.ok) {
        throw new Error(`Failed to discover printer: ${response.status} ${response.statusText}`)
      }
      const result = await response.json()
      if (result.elegooip) {
        setElegooip(result.elegooip)
      }
      setDiscoverSuccess(true)
      setTimeout(() => setDiscoverSuccess(false), 3000)
    } catch (err: any) {
      setError(`Error discovering printer: ${err.message || 'Unknown error'}`)
      console.error('Failed to discover printer:', err)
    } finally {
      setDiscovering(false)
    }
  }

  return (
    <div class="card" >


      {loading() ? (
        <p>Loading settings.. <span class="loading loading-spinner loading-xl"></span>.</p>
      ) : (
        <div>
          {error() && (
            <div role="alert" class="mb-4 alert alert-error">
              {error()}
            </div>
          )}

          {saveSuccess() && (
            <div role="alert" class="mb-4 alert alert-success">
              Settings saved successfully!
            </div>
          )}

          <h2 class="text-lg font-bold mb-4">Wifi Settings</h2>
          {apMode() && (
            <div>
              <fieldset class="fieldset ">
                <legend class="fieldset-legend">SSID</legend>
                <input
                  type="text"
                  id="ssid"
                  value={ssid()}
                  onInput={(e) => setSsid(e.target.value)}
                  placeholder="Enter WiFi network name..."
                  class="input"
                />
              </fieldset>


              <fieldset class="fieldset">
                <legend class="fieldset-legend">Password</legend>
                <input
                  type="password"
                  id="password"
                  value={password()}
                  onInput={(e) => setPassword(e.target.value)}
                  placeholder="Enter WiFi password..."
                  class="input"
                />
              </fieldset>


              <div role="alert" class="mt-4 alert alert-info alert-soft">
                <span>Note: after changing the wifi network you may need to enter a new IP address to get to this device. If the wifi connection fails, the device will revert to AP mode and you can reconnect by connecting to the Wifi network named ElegooXBTTSFS20. If your network supports MDNS discovery you can also find this device at <a class="link link-accent" href="http://ccxsfs20.local">
                  ccxsfs20.local</a></span>
              </div>
            </div>
          )
          }
          {
            !apMode() && (
              <button class="btn" onClick={() => setApMode(true)}>Change Wifi network</button>
            )
          }

          <h2 class="text-lg font-bold mb-4 mt-10">Device Settings</h2>


          <fieldset class="fieldset">
            <legend class="fieldset-legend">Elegoo Centauri Carbon IP Address</legend>
            <input
              type="text"
              id="elegooip"
              value={elegooip()}
              onInput={(e) => setElegooip(e.target.value)}
              placeholder="xxx.xxx.xxx.xxx"
              class="input"
            />
            <button class="btn mt-2" disabled={discovering()} onClick={handleDiscover}>
              {discovering() ? 'Discovering...' : 'Auto-detect printer'}
            </button>
            {discoverSuccess() && (
              <p class="label text-success">Printer IP detected and applied.</p>
            )}
          </fieldset>


          <fieldset class="fieldset">
            <legend class="fieldset-legend">Expected Flow Deficit Threshold (mm)</legend>
            <input
              type="number"
              id="expectedDeficit"
              value={expectedDeficit()}
              onInput={(e) => setExpectedDeficit(parseFloat(e.target.value) || 0)}
              min="0"
              max="50"
              step="0.1"
              class="input"
            />
            <p class="label">How many millimeters of requested filament (based on printer telemetry) can accumulate without matching sensor movement before pausing. Higher values add tolerance, lower values catch jams faster.</p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Expected Flow Window (ms)</legend>
            <input
              type="number"
              id="expectedWindow"
              value={expectedWindow()}
              onInput={(e) => setExpectedWindow(parseInt(e.target.value) || 500)}
              min="250"
              max="5000"
              step="250"
              class="input"
            />
            <p class="label">How long to keep unmatched expected filament before it is ignored (anti-jitter). Increase if your printer reports large filament bursts, decrease for faster jam detection.</p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Keep Expected Filament Forever</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="keepExpectedForever"
                checked={keepExpectedForever()}
                onChange={(e) => setKeepExpectedForever(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">When enabled, expected filament from SDCP is never pruned based on time; any requested filament remains outstanding until matched by sensor movement or overwritten by new telemetry.</span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Start Print Timeout</legend>
            <input
              type="number"
              id="startPrintTimeout"
              value={startPrintTimeout()}
              onInput={(e) => setStartPrintTimeout(parseInt(e.target.value) || 10000)}
              min="1000"
              max="60000"
              step="1000"
              class="input"
            />
            <p class="label">Time in milliseconds to wait after print starts before allowing pause on filament runout</p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Pause on Runout</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="pauseOnRunout"
                checked={pauseOnRunout()}
                onChange={(e) => setPauseOnRunout(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">Pause printing when filament runs out, rather than letting the Elegoo Centauri Carbon handle the runout</span>

            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Behavior when SDCP replies are lost</legend>
            <select
              id="sdcpLossBehavior"
              class="select"
              value={sdcpLossBehavior()}
              onChange={(e) => setSdcpLossBehavior(parseInt(e.target.value) || 2)}
            >
              <option value={1}>Pause print when SDCP replies stop</option>
              <option value={2}>Disable detection until SDCP replies return</option>
            </select>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Developer Mode</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="devMode"
                checked={devMode()}
                onChange={(e) => setDevMode(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">When enabled, pause commands to the printer are suppressed but detection and logging still occur.</span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Verbose Flow Logging</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="verboseLogging"
                checked={verboseLogging()}
                onChange={(e) => setVerboseLogging(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">When enabled, logs detailed filament flow data (movement pulses, expected vs actual, deficit) to the Logs tab.</span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Flow Summary Logging (1 line/sec)</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="flowSummaryLogging"
                checked={flowSummaryLogging()}
                onChange={(e) => setFlowSummaryLogging(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">When enabled (with verbose logging off), emits a single condensed flow line about once per second (expected, actual, deficit, pulses, ratio) instead of per-pulse debug logs.</span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Enabled</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="enabled"
                checked={enabled()}
                onChange={(e) => setEnabled(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">When unchecked, it will completely disable pausing, useful for prints with ironing</span>

            </label>
          </fieldset>

          <button
            class="btn btn-accent btn-soft mt-10"
            onClick={handleSave}
          >
            Save Settings
          </button>
        </div >
      )
      }
    </div >
  )
}

export default Settings 
