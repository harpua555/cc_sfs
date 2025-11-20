import { createSignal, onMount, onCleanup, createEffect } from 'solid-js'

interface LogEntry {
  uuid: string
  timestamp: number
  message: string
}

function Logs() {
  const [loading, setLoading] = createSignal(true)
  const [logs, setLogs] = createSignal<LogEntry[]>([])
  const [error, setError] = createSignal('')
  const [isAtBottom, setIsAtBottom] = createSignal(true)
  let intervalId: number | null = null
  let logContainerRef: HTMLDivElement | undefined

  const formatTimestamp = (timestamp: number): string => {
    try {
      const date = new Date(timestamp * 1000)
      return date.toLocaleString()
    } catch (err) {
      return timestamp.toString()
    }
  }

  const fetchLogs = async () => {
    try {
      const response = await fetch('/api/logs_text')
      if (!response.ok) {
        throw new Error(`Failed to fetch logs: ${response.status} ${response.statusText}`)
      }
      const text = await response.text()
      const parsedLogs = text
        .split('\n')
        .map((line, index) => {
          const trimmed = line.trim()
          if (!trimmed) return null
          const firstSpace = trimmed.indexOf(' ')
          if (firstSpace < 0) {
            return { uuid: `${index}-${trimmed}`, timestamp: 0, message: trimmed } as LogEntry
          }
          const ts = parseInt(trimmed.slice(0, firstSpace), 10)
          return {
            uuid: `${ts}-${index}-${trimmed.slice(firstSpace + 1)}`,
            timestamp: isNaN(ts) ? 0 : ts,
            message: trimmed.slice(firstSpace + 1)
          } as LogEntry
        })
        .filter((entry): entry is LogEntry => entry !== null)

      setLogs(parsedLogs)
      setError('')
      setLoading(false)
    } catch (err: any) {
      setError(`Error fetching logs: ${err.message || 'Unknown error'}`)
      console.error('Failed to fetch logs:', err)
      setLoading(false)
    }
  }

  const startAutoRefresh = () => {
    if (intervalId) clearInterval(intervalId)
    intervalId = setInterval(fetchLogs, 5000) // Refresh every 5 seconds
  }

  const stopAutoRefresh = () => {
    if (intervalId) {
      clearInterval(intervalId)
      intervalId = null
    }
  }

  const scrollToBottom = () => {
    if (logContainerRef) {
      logContainerRef.scrollTop = logContainerRef.scrollHeight
    }
  }

  const checkIfAtBottom = () => {
    if (logContainerRef) {
      const { scrollTop, scrollHeight, clientHeight } = logContainerRef
      // Consider "at bottom" if within 5 pixels of the bottom
      const atBottom = scrollHeight - scrollTop - clientHeight < 5
      setIsAtBottom(atBottom)
    }
  }

  const handleScroll = () => {
    checkIfAtBottom()
  }

  // Auto-scroll when logs change, but only if user is at bottom
  createEffect(() => {
    logs()
    if (isAtBottom()) {
      setTimeout(scrollToBottom, 0)
    }
  })

  onMount(async () => {
    setLoading(true)
    await fetchLogs()
    startAutoRefresh()
    // Ensure we start at the bottom
    setTimeout(() => {
      scrollToBottom()
      setIsAtBottom(true)
    }, 100)
  })

  onCleanup(() => {
    stopAutoRefresh()
  })

  return (
    <div>
      {error() && (
        <div role="alert" class="mb-4 alert alert-error">
          {error()}
        </div>
      )}


      {loading() && logs().length === 0 ? (
        <p><span class="loading loading-spinner loading-xl"></span></p>
      ) : (
        <div class="max-h-160 overflow-y-auto bg-black text-green-400 p-4 rounded font-mono text-sm border border-gray-600" ref={el => logContainerRef = el} onScroll={handleScroll}>
          {logs().length === 0 ? (
            <div class="text-center text-green-400/70">No logs available</div>
          ) : (
            logs().map((log) => (
              <div class="whitespace-pre-wrap">
                <span class="text-green-800">{formatTimestamp(log.timestamp)}:</span> {log.message}
              </div>
            ))
          )}
        </div>
      )}

      <div class="mt-4 flex items-center justify-between flex-wrap gap-2 text-sm text-base-content/70">
        <p>Logs are automatically refreshed every 5 seconds.</p>
        <a class="btn btn-xs btn-outline btn-primary" href="/api/logs_text" download="sfs-logs.txt">
          Download full log
        </a>
      </div>
    </div>
  )
}

export default Logs 
