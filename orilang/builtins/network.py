import requests

def http_get(url, timeout=10):
    """
    Performs an HTTP GET request with proper error handling and timeouts.
    
    Args:
        url (str): Target URL
        timeout (int): Request timeout in seconds (default: 10)
    
    Returns:
        dict: Structured response with either 'success' data or 'error' details
    """
    try:
        response = requests.get(url, timeout=timeout)
        response.raise_for_status()
        
        return {
            'status': 'success',
            'data': response.json(),
            'headers': dict(response.headers),
            'status_code': response.status_code
        }
    except requests.exceptions.Timeout:
        return {
            'status': 'error',
            'message': 'Request timed out',
            'error_type': 'timeout',
            'url': url
        }
    except requests.exceptions.ConnectionError:
        return {
            'status': 'error',
            'message': 'Failed to connect to server',
            'error_type': 'connection_error',
            'url': url
        }
    except requests.exceptions.HTTPError as e:
        return {
            'status': 'error',
            'message': f'HTTP error: {str(e)}',
            'error_type': 'http_error',
            'status_code': e.response.status_code if e.response else None,
            'url': url
        }
    except Exception as e:
        return {
            'status': 'error',
            'message': f'Unexpected error: {str(e)}',
            'error_type': 'unknown',
            'url': url
        }
