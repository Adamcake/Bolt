import { writable } from 'svelte/store';

export enum Severity {
	info = 'info',
	warn = 'warn',
	error = 'error'
}

export type Log = {
	severity: Severity;
	date: Date;
	content: string;
};

class Logger {
	// TODO: make this a $state variable with svelte5
	logs = writable<Log[]>([]);

	private _addLog(severity: Severity, content: string) {
		this.logs.update((logs) => {
			logs.unshift({
				date: new Date(),
				severity,
				content
			});
			return logs;
		});
	}

	info(content: string) {
		this._addLog(Severity.info, content);
	}

	warn(content: string) {
		this._addLog(Severity.warn, content);
	}

	error(content: string) {
		this._addLog(Severity.error, content);
	}
}

export const logger = new Logger();
