import { createSignal, onMount } from 'solid-js'

function Settings() {
  const [ssid, setSsid] = createSignal('')
  const [password, setPassword] = createSignal('')
  const [elegooip, setElegooip] = createSignal('')
  const [startPrintTimeout, setStartPrintTimeout] = createSignal(10000)
  const [detectionLength, setDetectionLength] = createSignal(10.0)  // New unified setting
  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal('')
  const [saveSuccess, setSaveSuccess] = createSignal(false)
  const [apMode, setApMode] = createSignal<boolean | null>(null);
  const [pauseOnRunout, setPauseOnRunout] = createSignal(true);
  const [enabled, setEnabled] = createSignal(true);
  const [sdcpLossBehavior, setSdcpLossBehavior] = createSignal(2);
  const [devMode, setDevMode] = createSignal(false);
  const [verboseLogging, setVerboseLogging] = createSignal(false);
  const [flowSummaryLogging, setFlowSummaryLogging] = createSignal(false);
  const [discovering, setDiscovering] = createSignal(false);
  const [discoverSuccess, setDiscoverSuccess] = createSignal(false);
  const [movementPerPulse, setMovementPerPulse] = createSignal(2.88)  // Correct default
  const [flowTelemetryStaleMs, setFlowTelemetryStaleMs] = createSignal(1000)
  const [uiRefreshIntervalMs, setUiRefreshIntervalMs] = createSignal(1000)
  const [detectionGracePeriodMs, setDetectionGracePeriodMs] = createSignal(500)
  const [trackingMode, setTrackingMode] = createSignal(1)  // 0=Cumulative, 1=Windowed, 2=EWMA
  const [trackingWindowMs, setTrackingWindowMs] = createSignal(5000)
  const [trackingEwmaAlpha, setTrackingEwmaAlpha] = createSignal(0.3)
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
      // Handle both new and old setting names for migration
      setDetectionLength(settings.detection_length_mm !== undefined ? settings.detection_length_mm :
                         settings.expected_deficit_mm !== undefined ? settings.expected_deficit_mm : 10.0)
      setSdcpLossBehavior(settings.sdcp_loss_behavior !== undefined ? settings.sdcp_loss_behavior : 2)
      setDevMode(settings.dev_mode !== undefined ? settings.dev_mode : false)
      setVerboseLogging(settings.verbose_logging !== undefined ? settings.verbose_logging : false)
      setFlowSummaryLogging(settings.flow_summary_logging !== undefined ? settings.flow_summary_logging : false)
      setMovementPerPulse(settings.movement_mm_per_pulse !== undefined ? settings.movement_mm_per_pulse : 2.88)
      setFlowTelemetryStaleMs(settings.flow_telemetry_stale_ms !== undefined ? settings.flow_telemetry_stale_ms : 1000)
      setUiRefreshIntervalMs(settings.ui_refresh_interval_ms !== undefined ? settings.ui_refresh_interval_ms : 1000)
      setDetectionGracePeriodMs(settings.detection_grace_period_ms !== undefined ? settings.detection_grace_period_ms : 500)
      setTrackingMode(settings.tracking_mode !== undefined ? settings.tracking_mode : 1)
      setTrackingWindowMs(settings.tracking_window_ms !== undefined ? settings.tracking_window_ms : 5000)
      setTrackingEwmaAlpha(settings.tracking_ewma_alpha !== undefined ? settings.tracking_ewma_alpha : 0.3)

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
        detection_length_mm: detectionLength(),  // New unified setting
        detection_grace_period_ms: detectionGracePeriodMs(),
        tracking_mode: trackingMode(),
        tracking_window_ms: trackingWindowMs(),
        tracking_ewma_alpha: trackingEwmaAlpha(),
        sdcp_loss_behavior: sdcpLossBehavior(),
        flow_telemetry_stale_ms: flowTelemetryStaleMs(),
        ui_refresh_interval_ms: uiRefreshIntervalMs(),
        dev_mode: devMode(),
        verbose_logging: verboseLogging(),
        flow_summary_logging: flowSummaryLogging(),
        movement_mm_per_pulse: movementPerPulse(),
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
        setDiscoverSuccess(true)
        setTimeout(() => setDiscoverSuccess(false), 3000)
      } else if (result.error) {
        setError(result.error)
      } else {
        setError('Discovery did not return a printer IP.')
      }
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
            <legend class="fieldset-legend">Detection Length (mm)</legend>
            <input
              type="number"
              id="detectionLength"
              value={detectionLength()}
              onInput={(e) => setDetectionLength(parseFloat(e.target.value) || 0)}
              onBlur={() => setDetectionLength(parseFloat(detectionLength().toFixed(2)))}
              min="1"
              max="30"
              step="0.5"
              class="input"
            />
            <p class="label">
              Distance threshold for jam detection (Klipper-style). When expected extrusion exceeds actual sensor movement by this distance, a jam is detected.
              <br />
              <strong>Recommended:</strong> 7-15mm. Lower = faster detection, Higher = more tolerant. Klipper default is 7mm.
            </p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Filament Movement per Pulse (mm)</legend>
            <input
              type="number"
              id="movementPerPulse"
              value={movementPerPulse()}
              onInput={(e) => setMovementPerPulse(parseFloat(e.target.value) || 0)}
              onBlur={() => setMovementPerPulse(parseFloat(movementPerPulse().toFixed(3)))}
              min="0.1"
              max="10"
              step="0.01"
              class="input"
            />
            <p class="label">
              Filament distance per sensor pulse. <strong>Default: 2.88mm</strong> (SFS 2.0 spec). Adjust only if calibration shows different values.
            </p>
          </fieldset>

          <h2 class="text-lg font-bold mb-4 mt-10">Tracking Algorithm Settings</h2>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Tracking Mode</legend>
            <select
              id="trackingMode"
              class="select"
              value={trackingMode()}
              onChange={(e) => setTrackingMode(parseInt(e.target.value) || 1)}
            >
              <option value={0}>Mode 0: Cumulative (Legacy - has drift)</option>
              <option value={1}>Mode 1: Windowed (Klipper-style - RECOMMENDED)</option>
              <option value={2}>Mode 2: EWMA (Memory-efficient)</option>
            </select>
            <p class="label">
              <strong>Mode 1 (Windowed)</strong> is recommended for production use. It uses a sliding time window to prevent calibration drift.
              <br />
              <strong>Mode 2 (EWMA)</strong> is a memory-efficient alternative using exponential averaging.
              <br />
              <strong>Mode 0 (Cumulative)</strong> is the legacy algorithm with drift issues on long prints.
              <br />
              See <a href="https://github.com/yourusername/cc_sfs/blob/main/TRACKING_ALGORITHMS.md" target="_blank" class="link link-accent">TRACKING_ALGORITHMS.md</a> for detailed comparison.
            </p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Detection Grace Period (ms)</legend>
            <input
              type="number"
              id="detectionGracePeriodMs"
              value={detectionGracePeriodMs()}
              onInput={(e) => setDetectionGracePeriodMs(parseInt(e.target.value) || 500)}
              min="0"
              max="2000"
              step="100"
              class="input"
            />
            <p class="label">
              Time to wait after move command before checking for jams (handles SDCP look-ahead).
              <br />
              <strong>Default: 500ms</strong>. Increase to 750-1000ms for slower prints, decrease to 300ms for fast prints.
            </p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Windowed Mode: Window Size (ms)</legend>
            <input
              type="number"
              id="trackingWindowMs"
              value={trackingWindowMs()}
              onInput={(e) => setTrackingWindowMs(parseInt(e.target.value) || 5000)}
              min="1000"
              max="10000"
              step="1000"
              class="input"
              disabled={trackingMode() !== 1}
            />
            <p class="label">
              {trackingMode() === 1
                ? "Sliding time window size for Mode 1 (Windowed). Default: 5000ms (5 seconds). Larger window = more drift resistance but slower response."
                : "Only applicable when Tracking Mode is set to Mode 1 (Windowed)."}
            </p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">EWMA Mode: Alpha (smoothing factor)</legend>
            <input
              type="number"
              id="trackingEwmaAlpha"
              value={trackingEwmaAlpha()}
              onInput={(e) => setTrackingEwmaAlpha(parseFloat(e.target.value) || 0.3)}
              onBlur={() => setTrackingEwmaAlpha(parseFloat(trackingEwmaAlpha().toFixed(2)))}
              min="0.1"
              max="0.5"
              step="0.05"
              class="input"
              disabled={trackingMode() !== 2}
            />
            <p class="label">
              {trackingMode() === 2
                ? "Smoothing factor for Mode 2 (EWMA). Default: 0.3. Lower (0.1-0.2) = smoother, Higher (0.4-0.5) = more responsive."
                : "Only applicable when Tracking Mode is set to Mode 2 (EWMA)."}
            </p>
          </fieldset>

          <h2 class="text-lg font-bold mb-4 mt-10">Other Settings</h2>

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
            <legend class="fieldset-legend">SDCP connection time considered lost (ms)</legend>
            <input
              type="number"
              id="flowTelemetryStaleMs"
              value={flowTelemetryStaleMs()}
              onInput={(e) => setFlowTelemetryStaleMs(parseInt(e.target.value) || 1000)}
              min="250"
              max="60000"
              step="250"
              class="input"
            />
            <p class="label">
              How long the printer can go without updated extrusion telemetry before it is considered lost for jam detection purposes.
            </p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Web UI refresh interval (ms)</legend>
            <input
              type="number"
              id="uiRefreshIntervalMs"
              value={uiRefreshIntervalMs()}
              onInput={(e) => setUiRefreshIntervalMs(parseInt(e.target.value) || 1000)}
              min="250"
              max="5000"
              step="250"
              class="input"
            />
            <p class="label">
              How often the Status page polls the ESP for updated sensor and printer information.
            </p>
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
