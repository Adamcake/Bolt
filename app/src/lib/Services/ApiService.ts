const BASE_URL = 'bolt-internal';

export class ApiService {
	static async get(route: string, params?: URLSearchParams): Promise<Response> {
		let parsedParams = '';
		if (params instanceof URLSearchParams) {
			parsedParams += '?' + params.toString();
		}
		const url = `${BASE_URL}/${route + parsedParams}`;
		const response = await fetch(url, {
			method: 'GET',
			headers: {
				'Content-Type': 'application/json'
			}
		});

		return response;
	}

	static async post(route: string, params: unknown): Promise<Response> {
		const url = `${BASE_URL}/${route}`;
		const response = await fetch(url, {
			method: 'POST',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify(params)
		});

		return response;
	}

	static async put(route: string, body: unknown): Promise<Response> {
		const url = `${BASE_URL}/${route}`;
		const response = await fetch(url, {
			method: 'PUT',
			headers: {
				'Content-Type': 'application/json'
			},
			body: JSON.stringify(body)
		});

		return response;
	}

	static async delete(route: string): Promise<Response> {
		const url = `${BASE_URL}/${route}`;
		const response = await fetch(url, {
			method: 'DELETE',
			headers: {
				'Content-Type': 'application/json'
			}
		});

		return response;
	}
}
