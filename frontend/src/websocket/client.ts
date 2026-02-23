import { API_BASE_URL } from "@/api/http";
import type { IngestRejectedEvent, TelemetryIngestedEvent } from "@/types/contracts";

const envWsBase = import.meta.env.VITE_WS_BASE_URL as string | undefined;

const wsBaseUrl = (() => {
  if (envWsBase) {
    return envWsBase.replace(/\/$/, "");
  }
  return API_BASE_URL.replace(/^http/, "ws");
})();

interface SocketHandlers<TEvent> {
  onMessage: (event: TEvent) => void;
  onOpen?: () => void;
  onClose?: () => void;
  onError?: (error: Error) => void;
}

const connectSocket = <TEvent>(path: string, handlers: SocketHandlers<TEvent>): WebSocket => {
  const socket = new WebSocket(`${wsBaseUrl}${path}`);

  socket.addEventListener("open", () => {
    handlers.onOpen?.();
  });

  socket.addEventListener("message", (messageEvent) => {
    try {
      handlers.onMessage(JSON.parse(messageEvent.data as string) as TEvent);
    } catch {
      handlers.onError?.(new Error("Failed to parse websocket payload as JSON."));
      return;
    }
  });

  socket.addEventListener("error", () => {
    handlers.onError?.(new Error("WebSocket connection error."));
  });

  socket.addEventListener("close", () => {
    handlers.onClose?.();
  });

  return socket;
};

export const connectTelemetrySocket = (handlers: SocketHandlers<TelemetryIngestedEvent>): WebSocket =>
  connectSocket<TelemetryIngestedEvent>("/ws/telemetry", handlers);

export const connectAlertSocket = (handlers: SocketHandlers<IngestRejectedEvent>): WebSocket =>
  connectSocket<IngestRejectedEvent>("/ws/alerts", handlers);
