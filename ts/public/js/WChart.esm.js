/**
 * WChart - A flexible Chart.js wrapper with comprehensive configuration options
 */
/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║  Copyright (c) 2025 Sylvain Guinebert - Paris, France         ║
 * ║  MIT License                                                  ║
 * ╚═══════════════════════════════════════════════════════════════╝
 */
import { Chart, _adapters } from '../vendor/chartjs/chartjs/auto/auto.js';
import  zoomPlugin  from '../vendor/chartjs/chartjs-plugin-zoom.esm.js';
Chart.register(zoomPlugin);

// Register a simple date adapter
_adapters._date.override({
  _id: 'date-fns', // ID for the adapter
  formats: function() {
    return {
      datetime: 'MMM d, yyyy, h:mm:ss a',
      millisecond: 'h:mm:ss.SSS a',
      second: 'h:mm:ss a',
      minute: 'h:mm a',
      hour: 'ha',
      day: 'MMM d',
      week: 'PP',
      month: 'MMM yyyy',
      quarter: 'QQQ - yyyy',
      year: 'yyyy'
    };
  },
  parse: function(value) {
    return new Date(value);
  },
  format: function(date, format) {
    return date.toLocaleString();
  },
  add: function(time, amount, unit) {
    const date = new Date(time);
    switch (unit) {
      case 'millisecond': date.setMilliseconds(date.getMilliseconds() + amount); break;
      case 'second': date.setSeconds(date.getSeconds() + amount); break;
      case 'minute': date.setMinutes(date.getMinutes() + amount); break;
      case 'hour': date.setHours(date.getHours() + amount); break;
      case 'day': date.setDate(date.getDate() + amount); break;
      case 'week': date.setDate(date.getDate() + amount * 7); break;
      case 'month': date.setMonth(date.getMonth() + amount); break;
      case 'quarter': date.setMonth(date.getMonth() + amount * 3); break;
      case 'year': date.setFullYear(date.getFullYear() + amount); break;
    }
    return date.getTime();
  },
  diff: function(max, min, unit) {
    const diff = max - min;
    switch (unit) {
      case 'millisecond': return diff;
      case 'second': return diff / 1000;
      case 'minute': return diff / (1000 * 60);
      case 'hour': return diff / (1000 * 60 * 60);
      case 'day': return diff / (1000 * 60 * 60 * 24);
      case 'week': return diff / (1000 * 60 * 60 * 24 * 7);
      case 'month': return diff / (1000 * 60 * 60 * 24 * 30);
      case 'quarter': return diff / (1000 * 60 * 60 * 24 * 90);
      case 'year': return diff / (1000 * 60 * 60 * 24 * 365);
    }
  },
  startOf: function(time, unit) {
    const date = new Date(time);
    switch (unit) {
      case 'second': date.setMilliseconds(0); break;
      case 'minute': date.setSeconds(0); date.setMilliseconds(0); break;
      case 'hour': date.setMinutes(0); date.setSeconds(0); date.setMilliseconds(0); break;
      case 'day': date.setHours(0); date.setMinutes(0); date.setSeconds(0); date.setMilliseconds(0); break;
      case 'week': 
        date.setDate(date.getDate() - date.getDay());
        date.setHours(0); date.setMinutes(0); date.setSeconds(0); date.setMilliseconds(0);
        break;
      case 'month': date.setDate(1); date.setHours(0); date.setMinutes(0); date.setSeconds(0); date.setMilliseconds(0); break;
      case 'quarter':
        const quarter = Math.floor(date.getMonth() / 3);
        date.setMonth(quarter * 3);
        date.setDate(1);
        date.setHours(0); date.setMinutes(0); date.setSeconds(0); date.setMilliseconds(0);
        break;
      case 'year': date.setMonth(0); date.setDate(1); date.setHours(0); date.setMinutes(0); date.setSeconds(0); date.setMilliseconds(0); break;
    }
    return date.getTime();
  }
});

export default class WChart {
  #config;
  #el;
  /** @type {HTMLCanvasElement} */
  #canvas;
  #chart;
  #chartConfig;
  #resizeObserver;

  /**
   * Create a new chart instance
   * @param {Object} config - Configuration object
   * @param {string|HTMLElement} config.el - CSS selector or DOM element for the chart canvas
   * @param {string} config.type - Chart type ('line', 'bar', 'pie', 'radar', 'polarArea', 'doughnut', 'scatter', 'bubble')
   * @param {Object} config.data - Chart.js data configuration
   * @param {Object} config.options - Chart.js options configuration
   * @param {Function} config.onInit - Callback when chart is initialized
   */
  constructor(el, config) {
    this.#config = config;
    this.#el = typeof el === 'string' ? document.getElementById(el) : el;

    if (!this.#el || !(this.#el instanceof HTMLElement)) {
      console.error('WChart: Invalid element provided for chart initialization');
      return;
    }
    
    this.#setupContainer();
    
    this.#chartConfig = {
      type: config.type || 'line',
      data: config.data || { datasets: [] },
      options: this.#setupDefaultOptions(config.options || {})
    };
    
    this.#chart = new Chart(this.#canvas.getContext('2d'), this.#chartConfig);
    
    this.#setupResizeHandler();
    
    if (typeof config.onInit === 'function') {
      config.onInit(this);
    }
  }
  
  /**
   * Sets up container for the chart canvas
   * @private
   */
  #setupContainer() {
    this.#el.className = 'wt-chart-container';
    this.#el.style.position = 'relative';
    this.#el.style.width = '100%';
    this.#el.style.height = '100%';

    // Create canvas element
    this.#canvas = document.createElement('canvas');
    this.#canvas.style.width = '100%';
    this.#canvas.style.height = '100%';

    this.#el.appendChild(this.#canvas);

    const dpr = window.devicePixelRatio || 1;
    if (dpr > 1) {
      const rect = this.#canvas.getBoundingClientRect();
      this.#canvas.width = rect.width * dpr;
      this.#canvas.height = rect.height * dpr;
      const ctx = this.#canvas.getContext('2d');
      ctx.scale(dpr, dpr);
    }
  }
  
  /**
   * Sets up default options with sensible values
   * @private
   */
  #setupDefaultOptions(userOptions) {
    const defaultOptions = {
      responsive: true,
      maintainAspectRatio: false,
      animation: {
        duration: 400
      },
      // Default zoom plugin configuration
      plugins: {
        zoom: {
          pan: {
            enabled: false,
            mode: 'xy'
          },
          zoom: {
            wheel: {
              enabled: false
            },
            pinch: {
              enabled: false
            },
            mode: 'xy'
          }
        }
      }
    };
    
    return { ...defaultOptions, ...userOptions };
  }
  
  /**
   * Sets up a resize observer to handle canvas resizing
   * @private
   */
  #setupResizeHandler() {
    this.#resizeObserver = new ResizeObserver(() => {
      this.#chart.resize();
    });
    
    this.#resizeObserver.observe(this.#canvas.parentElement);
  }


  /**
   * Configure zoom and pan functionality
   * @param {Object} config - Zoom configuration
   * @param {boolean} config.enableZoom - Enable/disable wheel/pinch zooming
   * @param {boolean} config.enablePan - Enable/disable panning
   * @param {string} config.mode - Zoom/pan mode ('x', 'y', or 'xy')
   * @param {number} config.wheelSensitivity - Mouse wheel sensitivity (default: 0.1)
   * @param {boolean} animate - Whether to animate the update
   */
  configureZoomPan(config, animate = true) {
    const options = this.#chart.options;
    
    if (!options.plugins) options.plugins = {};
    if (!options.plugins.zoom) options.plugins.zoom = {};
    
    // Configure zoom
    if (!options.plugins.zoom.zoom) options.plugins.zoom.zoom = {};
    if (!options.plugins.zoom.zoom.wheel) options.plugins.zoom.zoom.wheel = {};
    if (!options.plugins.zoom.zoom.pinch) options.plugins.zoom.zoom.pinch = {};
    
    if (config.enableZoom !== undefined) {
      options.plugins.zoom.zoom.wheel.enabled = !!config.enableZoom;
      options.plugins.zoom.zoom.pinch.enabled = !!config.enableZoom;
    }
    
    // Configure pan
    if (!options.plugins.zoom.pan) options.plugins.zoom.pan = {};
    
    if (config.enablePan !== undefined) {
      options.plugins.zoom.pan.enabled = !!config.enablePan;
    }
    
    // Configure mode
    if (config.mode && ['x', 'y', 'xy'].includes(config.mode)) {
      options.plugins.zoom.zoom.mode = config.mode;
      options.plugins.zoom.pan.mode = config.mode;
    }
    
    // Configure wheel sensitivity
    if (config.wheelSensitivity !== undefined) {
      options.plugins.zoom.zoom.wheel.speed = config.wheelSensitivity;
    }
    
    this.#chart.update(animate ? undefined : 0);
  }

  /**
   * Zoom the chart programmatically
   * @param {Object} config - Zoom configuration 
   * @param {number} config.x - X-axis zoom factor (1 = original scale)
   * @param {number} config.y - Y-axis zoom factor (1 = original scale)
   * @param {boolean} animate - Whether to animate the zoom
   */
  zoomChart(config, animate = true) {
    if (!this.#chart.zoom) {
      console.warn('WChart: Zoom plugin not available');
      return;
    }
    
    const duration = animate ? 500 : 0;
    
    if (config.x !== undefined || config.y !== undefined) {
      this.#chart.zoom({
        x: config.x || 1,
        y: config.y || 1,
        duration
      });
    }
  }

  /**
   * Pan the chart programmatically
   * @param {Object} config - Pan configuration
   * @param {number} config.x - X-axis pan amount in pixels
   * @param {number} config.y - Y-axis pan amount in pixels
   * @param {boolean} animate - Whether to animate the pan
   */
  panChart(config, animate = true) {
    if (!this.#chart.pan) {
      console.warn('WChart: Zoom plugin not available');
      return;
    }
    
    const duration = animate ? 500 : 0;
    
    if (config.x !== undefined || config.y !== undefined) {
      this.#chart.pan({
        x: config.x || 0,
        y: config.y || 0,
        duration
      });
    }
  }

  /**
   * Reset zoom and pan to initial state
   * @param {boolean} animate - Whether to animate the reset
   */
  resetZoomPan(animate = true) {
    if (!this.#chart.resetZoom) {
      console.warn('WChart: Zoom plugin not available');
      return;
    }
    
    const duration = animate ? 500 : 0;
    this.#chart.resetZoom(duration);
  }
  
  /**
   * Updates the chart data and redraws
   * @param {Object} data - New chart data
   * @param {boolean} animate - Whether to animate the transition
   */
  updateData(data, animate = true) {
    this.#chart.data = data;
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Updates a specific dataset by index
   * @param {number} index - Dataset index
   * @param {Object} data - New dataset data
   * @param {boolean} animate - Whether to animate the transition
   */
  updateDataset(index, data, animate = true) {
    if (index < 0 || index >= this.#chart.data.datasets.length) {
      console.error('WChart: Invalid dataset index');
      return;
    }
    
    Object.assign(this.#chart.data.datasets[index], data);
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Adds a new dataset to the chart
   * @param {Object} dataset - New dataset configuration
   * @param {boolean} animate - Whether to animate the transition
   */
  addDataset(dataset, animate = true) {
    this.#chart.data.datasets.push(dataset);
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Removes a dataset from the chart
   * @param {number} index - Dataset index to remove
   * @param {boolean} animate - Whether to animate the transition
   */
  removeDataset(index, animate = true) {
    if (index < 0 || index >= this.#chart.data.datasets.length) {
      console.error('WChart: Invalid dataset index');
      return;
    }
    
    this.#chart.data.datasets.splice(index, 1);
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Updates chart options
   * @param {Object} options - New chart options to merge
   * @param {boolean} animate - Whether to animate the transition
   */
  updateOptions(options, animate = true) {
    this.#chart.options = { ...this.#chart.options, ...options };
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Changes the chart type
   * @param {string} type - New chart type
   * @param {boolean} animate - Whether to animate the transition
   */
  setType(type, animate = true) {
    this.#chart.config.type = type;
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Updates labels for the chart
   * @param {Array} labels - New labels
   * @param {boolean} animate - Whether to animate the transition
   */
  setLabels(labels, animate = true) {
    this.#chart.data.labels = labels;
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Toggles dataset visibility
   * @param {number} index - Dataset index
   * @param {boolean} animate - Whether to animate the transition
   */
  toggleDatasetVisibility(index, animate = true) {
    if (index < 0 || index >= this.#chart.data.datasets.length) {
      console.error('WChart: Invalid dataset index');
      return;
    }
    
    const meta = this.#chart.getDatasetMeta(index);
    meta.hidden = !meta.hidden;
    this.#chart.update(animate ? undefined : 0);
  }
  
  /**
   * Export chart as image
   * @param {string} type - Image format (default: 'image/png')
   * @param {number} quality - Image quality for JPEG (0-1)
   * @returns {string} Data URL of the image
   */
  toDataURL(type = 'image/png', quality) {
    return this.#canvas.toDataURL(type, quality);
  }
  
  /**
   * Gets the raw Chart.js instance
   * @returns {Chart} The Chart.js instance
   */
  get chart() {
    return this.#chart;
  }
  
  /**
   * Destroy the chart instance and clean up
   */
  destroy() {
    if (this.#resizeObserver) {
      this.#resizeObserver.disconnect();
    }
    if (this.#chart) {
      this.#chart.destroy();
    }
  }
  
  /**
   * Helper method to generate color sets
   * @param {number} count - Number of colors needed
   * @param {number} opacity - Background opacity (0-1)
   * @returns {Array} Array of color objects
   */
  static generateColors(count, opacity = 0.7) {
    const colors = [
      { bg: 'rgba(52, 152, 219, {o})', border: 'rgba(52, 152, 219, 1)' },  // Blue
      { bg: 'rgba(46, 204, 113, {o})', border: 'rgba(46, 204, 113, 1)' },  // Green
      { bg: 'rgba(155, 89, 182, {o})', border: 'rgba(155, 89, 182, 1)' },  // Purple
      { bg: 'rgba(230, 126, 34, {o})', border: 'rgba(230, 126, 34, 1)' },  // Orange
      { bg: 'rgba(241, 196, 15, {o})', border: 'rgba(241, 196, 15, 1)' },  // Yellow
      { bg: 'rgba(231, 76, 60, {o})', border: 'rgba(231, 76, 60, 1)' }     // Red
    ];
    
    return Array.from({ length: count }, (_, i) => {
      const color = colors[i % colors.length];
      return {
        backgroundColor: color.bg.replace('{o}', opacity),
        borderColor: color.border
      };
    });
  }
}
