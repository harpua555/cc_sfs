import { createSignal, onMount, onCleanup } from 'solid-js'



const PRINT_STATUS_MAP = {
  0: 'Idle',
  1: 'Homing',
  2: 'Dropping',
  3: 'Exposing',
  4: 'Lifting',
  5: 'Pausing',
  6: 'Paused',
  7: 'Stopping',
  8: 'Stopped',
  9: 'Complete',
  10: 'File Checking',
  13: 'Printing',
  15: 'Unknown: 15',
  16: 'Heating',
  18: 'Unknown: 18',
  19: 'Unknown: 19',
  20: 'Bed Leveling',
  21: 'Unknown: 21',
}

function Status() {

  const [loading, setLoading] = createSignal(true)
  const [error, setError] = createSignal<string | null>(null)
  const [sensorStatus, setSensorStatus] = createSignal({
    stopped: false,
    filamentRunout: false,
    elegoo: {
      mainboardID: '',
      printStatus: 0,
      isPrinting: false,
      currentLayer: 0,
      totalLayer: 0,
      progress: 0,
      currentTicks: 0,
      totalTicks: 0,
      PrintSpeedPct: 0,
      isWebsocketConnected: false,
      expectedFilament: 0,
      actualFilament: 0,
      expectedDelta: 0,
      telemetryAvailable: false,
      currentDeficitMm: 0,
      deficitThresholdMm: 0,
      deficitRatio: 0,
      movementPulses: 0,
    }
  })

  const refreshSensorStatus = async () => {
    try {
      const response = await fetch('/sensor_status')
      if (!response.ok) {
        throw new Error(`Failed to load sensor status: ${response.status} ${response.statusText}`)
      }
      const data = await response.json()
      setSensorStatus(data)
      setError(null)
    } catch (err: any) {
      console.error('Failed to load sensor status:', err)
      setError(err?.message || 'Failed to load sensor status')
    } finally {
      setLoading(false)
    }
  }

  onMount(async () => {
    setLoading(true)
    await refreshSensorStatus()
    const intervalId = setInterval(refreshSensorStatus, 2500)

    onCleanup(() => {
      clearInterval(intervalId)
    })
  })

  return (
    <div>
      {loading() ? (
        <p><span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div>
          {error() && (
            <div role="alert" class="mb-4 alert alert-error">
              {error()}
            </div>
          )}
          <div class="stats w-full shadow bg-base-200">
            {sensorStatus().elegoo.isWebsocketConnected && <>
              <div class="stat">
                <div class="stat-title">Filament Stopped</div>
                <div class={`stat-value ${sensorStatus().stopped ? 'text-error' : 'text-success'}`}> {sensorStatus().stopped ? 'Yes' : 'No'}</div>
              </div>
              <div class="stat">
                <div class="stat-title">Filament Runout</div>
                <div class={`stat-value ${sensorStatus().filamentRunout ? 'text-error' : 'text-success'}`}> {sensorStatus().filamentRunout ? 'Yes' : 'No'}</div>
              </div>
            </>
            }
            <div class="stat">
              <div class="stat-title">Printer Connected</div>
              <div class={`stat-value ${sensorStatus().elegoo.isWebsocketConnected ? 'text-success' : 'text-error'}`}> {sensorStatus().elegoo.isWebsocketConnected ? 'Yes' : 'No'}</div>
            </div>
          </div>
          <div class="card w-full mt-8 bg-base-200 card-sm shadow-sm">
            <div class="card-body">
              <h2 class="card-title">More Information</h2>
              <div class="text-sm flex gap-4 flex-wrap">
                <div>
                  <h3 class="font-bold">Mainboard ID</h3>
                  <p>{sensorStatus().elegoo.mainboardID}</p>
                </div>
                <div>
                  <h3 class="font-bold">Currently Printing</h3>
                  <p>{sensorStatus().elegoo.isPrinting ? 'Yes' : 'No'}</p>
                </div>
                <div>
                  <h3 class="font-bold">Print Status</h3>
                  <p>{PRINT_STATUS_MAP[sensorStatus().elegoo.printStatus as keyof typeof PRINT_STATUS_MAP]}</p>
                </div>

                <div>
                  <h3 class="font-bold">Current Layer</h3>
                  <p>{sensorStatus().elegoo.currentLayer}</p>
                </div>
                <div>
                  <h3 class="font-bold">Total Layer</h3>
                  <p>{sensorStatus().elegoo.totalLayer}</p>
                </div>
                <div>
                  <h3 class="font-bold">Progress</h3>
                  <p>{sensorStatus().elegoo.progress}</p>
                </div>
                <div>
                  <h3 class="font-bold">Current Ticks</h3>
                  <p>{sensorStatus().elegoo.currentTicks}</p>
                </div>
                <div>
                  <h3 class="font-bold">Total Ticks</h3>
                  <p>{sensorStatus().elegoo.totalTicks}</p>
                </div>
              <div>
                  <h3 class="font-bold">Print Speed</h3>
                  <p>{sensorStatus().elegoo.PrintSpeedPct}</p>
                </div>
                <div>
                  <h3 class="font-bold">Telemetry Available</h3>
                  <p>{sensorStatus().elegoo.telemetryAvailable ? 'Yes' : 'No'}</p>
                </div>
                <div>
                  <h3 class="font-bold">Expected Filament (mm)</h3>
                  <p>{sensorStatus().elegoo.expectedFilament}</p>
                </div>
                <div>
                  <h3 class="font-bold">Actual Filament (mm)</h3>
                  <p>{sensorStatus().elegoo.actualFilament}</p>
                </div>
                <div>
                  <h3 class="font-bold">Last Expected Delta (mm)</h3>
                  <p>{sensorStatus().elegoo.expectedDelta}</p>
                </div>
                <div>
                  <h3 class="font-bold">Deficit (mm)</h3>
                  <p>{sensorStatus().elegoo.currentDeficitMm?.toFixed?.(3) ?? sensorStatus().elegoo.currentDeficitMm}</p>
                </div>
                <div>
                  <h3 class="font-bold">Deficit (% of threshold)</h3>
                  <p>{(sensorStatus().elegoo.deficitRatio * 100).toFixed(1)}%</p>
                </div>
                <div>
                  <h3 class="font-bold">Movement Pulses</h3>
                  <p>{sensorStatus().elegoo.movementPulses}</p>
                </div>
              </div>
            </div>
          </div>

        </div>
      )}
    </div>
  )
}

export default Status 
