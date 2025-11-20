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
  const [flowSummaryLogging, setFlowSummaryLogging] = createSignal(false);
  const [discovering, setDiscovering] = createSignal(false);
  const [discoverSuccess, setDiscoverSuccess] = createSignal(false);
  const [movementPerPulse, setMovementPerPulse] = createSignal(1.5)
  const [flowTelemetryStaleMs, setFlowTelemetryStaleMs] = createSignal(1000)
  const [uiRefreshIntervalMs, setUiRefreshIntervalMs] = createSignal(1000)
  const [zeroDeficitLogging, setZeroDeficitLogging] = createSignal(false)
  const [totalVsDeltaLogging, setTotalVsDeltaLogging] = createSignal(false)
  const [packetFlowLogging, setPacketFlowLogging] = createSignal(false)
  const [useTotalExtrusionDeficit, setUseTotalExtrusionDeficit] = createSignal(false)
  const [useTotalExtrusionBacklog, setUseTotalExtrusionBacklog] = createSignal(false)
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
      setFlowSummaryLogging(settings.flow_summary_logging !== undefined ? settings.flow_summary_logging : false)
      setMovementPerPulse(settings.movement_mm_per_pulse !== undefined ? settings.movement_mm_per_pulse : 1.5)
      setFlowTelemetryStaleMs(settings.flow_telemetry_stale_ms !== undefined ? settings.flow_telemetry_stale_ms : 1000)
      setUiRefreshIntervalMs(settings.ui_refresh_interval_ms !== undefined ? settings.ui_refresh_interval_ms : 1000)
      setZeroDeficitLogging(settings.zero_deficit_logging !== undefined ? settings.zero_deficit_logging : false)
      setTotalVsDeltaLogging(settings.total_vs_delta_logging !== undefined ? settings.total_vs_delta_logging : false)
      setPacketFlowLogging(settings.packet_flow_logging !== undefined ? settings.packet_flow_logging : false)
      setUseTotalExtrusionDeficit(settings.use_total_extrusion_deficit !== undefined ? settings.use_total_extrusion_deficit : false)
      setUseTotalExtrusionBacklog(settings.use_total_extrusion_backlog !== undefined ? settings.use_total_extrusion_backlog : false)

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
        flow_telemetry_stale_ms: flowTelemetryStaleMs(),
        ui_refresh_interval_ms: uiRefreshIntervalMs(),
          zero_deficit_logging: zeroDeficitLogging(),
          total_vs_delta_logging: totalVsDeltaLogging(),
          packet_flow_logging: packetFlowLogging(),
        use_total_extrusion_deficit: useTotalExtrusionDeficit(),
        use_total_extrusion_backlog: useTotalExtrusionBacklog(),
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
            <legend class="fieldset-legend">Expected Flow Deficit Threshold (mm)</legend>
            <input
              type="number"
              id="expectedDeficit"
              value={expectedDeficit()}
              onInput={(e) => setExpectedDeficit(parseFloat(e.target.value) || 0)}
              onBlur={() => setExpectedDeficit(parseFloat(expectedDeficit().toFixed(2)))}
              min="0"
              max="50"
              step="0.1"
              class="input"
            />
            <p class="label">
              How many millimeters of requested filament backlog (based on printer telemetry) are permitted to accumulate without matching sensor movement before a jam is considered.
              <br />
              Higher values add tolerance, lower values catch jams faster.
            </p>
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
            <p class="label">
              How long the filament deficit must remain above the threshold (hold time) before a jam is triggered. Increase to require a longer stall before pausing; decrease for a faster response.
            </p>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Filament Movement per Pulse (mm)</legend>
            <input
              type="number"
              id="movementPerPulse"
              value={movementPerPulse()}
              onInput={(e) => setMovementPerPulse(parseFloat(e.target.value) || 0)}
              onBlur={() => setMovementPerPulse(parseFloat(movementPerPulse().toFixed(2)))}
              min="0.1"
              max="10"
              step="0.01"
              class="input"
            />
            <p class="label">
              How many millimeters of filament correspond to a single SFS sensor pulse (edge). Adjust this to calibrate actual filament movement against your physical measurements.
            </p>
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
            <legend class="fieldset-legend">Explicit zero-deficit logging</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="zeroDeficitLogging"
                checked={zeroDeficitLogging()}
                onChange={(e) => setZeroDeficitLogging(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">
                When enabled, logs whenever the deficit is explicitly reset to 0mm (for example when tracking resets or telemetry becomes unavailable).
              </span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Total vs delta logging</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="totalVsDeltaLogging"
                checked={totalVsDeltaLogging()}
                onChange={(e) => setTotalVsDeltaLogging(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">
                When enabled, log the SDCP TotalExtrusion value alongside the cumulative telemetry deltas and the backlog so we can compare them.
              </span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Packet flow logging</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="packetFlowLogging"
                checked={packetFlowLogging()}
                onChange={(e) => setPacketFlowLogging(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">
                When enabled, log per-telemetry-packet stats (total, delta, backlog, pulses) with timestamps so we can spot missing frames.
              </span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Use total-extrusion backlog logic</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="useTotalBacklog"
                checked={useTotalExtrusionDeficit()}
                onChange={(e) => setUseTotalExtrusionDeficit(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">
                When enabled, accumulate outstanding expected filament as a single backlog and subtract mm-per-pulse immediately, instead of queueing deltas.
              </span>
            </label>
          </fieldset>

          <fieldset class="fieldset">
            <legend class="fieldset-legend">Use Total Extrusion Backlog Mode</legend>
            <label class="label cursor-pointer">
              <input
                type="checkbox"
                id="useTotalExtrusionBacklog"
                checked={useTotalExtrusionBacklog()}
                onChange={(e) => setUseTotalExtrusionBacklog(e.target.checked)}
                class="checkbox checkbox-accent"
              />
              <span class="label-text">
                When enabled, backlog is derived directly from the SDCP TotalExtrusion value (resetting to zero when tracking is reset or after a jam while the printer’s cumulative total keeps increasing).
              </span>
            </label>
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
