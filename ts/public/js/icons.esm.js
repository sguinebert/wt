import { library, icon, dom } from '../vendor/fontawesome/fontawesome-svg-core.js';

// Configure Font Awesome for production
//config.autoAddCss = false; // Prevent Font Awesome from auto-inserting CSS
//config.searchPseudoElements = false; // Performance improvement

// =====================================================================
// HOW TO ADD CUSTOM ICONS
// =====================================================================
// 1. Import icons below from '../vendor/fontawesome/free-solid-svg-icons.esm.js'
// 2. Add them to library.add() call in the same category or create a new one
// 3. Add them to the iconMap with your preferred name mapping
// 
// The bundler will only include icons that are actually imported (tree-shaking)
// Find available icons at: https://fontawesome.com/icons?d=gallery&s=solid
// =====================================================================
// Import file and folder icons
import { 
  faFolder, faFolderOpen, faFolderPlus,
  faFile, faFileAlt, faFileImage, faFileAudio, 
  faFileVideo, faFileArchive, faFilePdf, 
  faFileExcel, faFileWord, faFilePowerpoint, 
  faFileCode, faFileCsv, faFileContract,
  faFileDownload, faFileExport, faFileImport
} from '../vendor/fontawesome/free-solid-svg-icons.esm.js';//'@fortawesome/free-solid-svg-icons';

// Import UI navigation icons
import {
  faChevronDown, faChevronUp, faChevronLeft, faChevronRight,
  faCaretDown, faCaretUp, faCaretLeft, faCaretRight,
  faArrowUp, faArrowDown, faArrowLeft, faArrowRight,
  faAngleDoubleLeft, faAngleDoubleRight, faAngleLeft, faAngleRight
} from '../vendor/fontawesome/free-solid-svg-icons.esm.js';//'@fortawesome/free-solid-svg-icons';

// Import action icons
import {
  faPlus, faMinus, faEdit, faTrash, faSave,
  faCopy, faCut, faPaste, faUndo, faRedo,
  faUpload, faDownload, faLink, faUnlink, faEye, faEyeSlash
} from '../vendor/fontawesome/free-solid-svg-icons.esm.js';//'@fortawesome/free-solid-svg-icons';

// Import UI element icons
import {
  faCheck, faCheckCircle, faCheckSquare, faSquare,
  faToggleOn, faToggleOff, faSliders, faSort,
  faSortUp, faSortDown, faFilter, faList, faTable
} from '../vendor/fontawesome/free-solid-svg-icons.esm.js';//'@fortawesome/free-solid-svg-icons';

// Import functional icons
import {
  faSearch, faUser, faUsers, faHome, faCog, faCogs,
  faBell, faCalendar, faClock, faMapMarker,
  faInfo, faInfoCircle, faQuestion, faQuestionCircle,
  faExclamation, faExclamationTriangle, faExclamationCircle
} from '../vendor/fontawesome/free-solid-svg-icons.esm.js';//'@fortawesome/free-solid-svg-icons';

// Add all the icons to the library
library.add(
  // Files and folders
  faFolder, faFolderOpen, faFolderPlus,
  faFile, faFileAlt, faFileImage, faFileAudio, 
  faFileVideo, faFileArchive, faFilePdf, 
  faFileExcel, faFileWord, faFilePowerpoint,
  faFileCode, faFileCsv, faFileContract,
  faFileDownload, faFileExport, faFileImport,
  
  // Navigation
  faChevronDown, faChevronUp, faChevronLeft, faChevronRight,
  faCaretDown, faCaretUp, faCaretLeft, faCaretRight,
  faArrowUp, faArrowDown, faArrowLeft, faArrowRight,
  faAngleDoubleLeft, faAngleDoubleRight, faAngleLeft, faAngleRight,
  
  // Actions
  faPlus, faMinus, faEdit, faTrash, faSave,
  faCopy, faCut, faPaste, faUndo, faRedo,
  faUpload, faDownload, faLink, faUnlink, faEye, faEyeSlash,
  
  // UI Elements
  faCheck, faCheckCircle, faCheckSquare, faSquare,
  faToggleOn, faToggleOff, faSliders, faSort,
  faSortUp, faSortDown, faFilter, faList, faTable,
  
  // Functional
  faSearch, faUser, faUsers, faHome, faCog, faCogs,
  faBell, faCalendar, faClock, faMapMarker,
  faInfo, faInfoCircle, faQuestion, faQuestionCircle,
  faExclamation, faExclamationTriangle, faExclamationCircle
);

// Complete icon mapping
const iconMap = {
  // Files and folders
  'folder': faFolder,
  'folder-open': faFolderOpen,
  'folder-plus': faFolderPlus,
  'file': faFile,
  'file-text': faFileAlt,
  'file-image': faFileImage,
  'file-audio': faFileAudio,
  'file-video': faFileVideo,
  'file-archive': faFileArchive,
  'file-pdf': faFilePdf,
  'file-excel': faFileExcel,
  'file-word': faFileWord,
  'file-powerpoint': faFilePowerpoint,
  'file-code': faFileCode,
  'file-csv': faFileCsv,
  'file-contract': faFileContract,
  'file-download': faFileDownload,
  'file-export': faFileExport,
  'file-import': faFileImport,
  
  // Navigation
  'chevron-down': faChevronDown,
  'chevron-up': faChevronUp,
  'chevron-left': faChevronLeft,
  'chevron-right': faChevronRight,
  'caret-down': faCaretDown,
  'caret-up': faCaretUp,
  'caret-left': faCaretLeft,
  'caret-right': faCaretRight,
  'arrow-up': faArrowUp,
  'arrow-down': faArrowDown,
  'arrow-left': faArrowLeft,
  'arrow-right': faArrowRight,
  'angle-double-left': faAngleDoubleLeft,
  'angle-double-right': faAngleDoubleRight,
  'angle-left': faAngleLeft,
  'angle-right': faAngleRight,
  
  // Actions
  'plus': faPlus,
  'minus': faMinus,
  'edit': faEdit,
  'trash': faTrash,
  'save': faSave,
  'copy': faCopy,
  'cut': faCut,
  'paste': faPaste,
  'undo': faUndo,
  'redo': faRedo,
  'upload': faUpload,
  'download': faDownload,
  'link': faLink,
  'unlink': faUnlink,
  'eye': faEye,
  'eye-slash': faEyeSlash,
  
  // UI Elements
  'check': faCheck,
  'check-circle': faCheckCircle,
  'check-square': faCheckSquare,
  'square': faSquare,
  'toggle-on': faToggleOn,
  'toggle-off': faToggleOff,
  'sliders': faSliders,
  'sort': faSort,
  'sort-up': faSortUp,
  'sort-down': faSortDown,
  'filter': faFilter,
  'list': faList,
  'table': faTable,
  
  // Functional
  'search': faSearch,
  'user': faUser,
  'users': faUsers,
  'home': faHome,
  'cog': faCog,
  'cogs': faCogs,
  'bell': faBell,
  'calendar': faCalendar,
  'clock': faClock,
  'map-marker': faMapMarker,
  'info': faInfo,
  'info-circle': faInfoCircle,
  'question': faQuestion,
  'question-circle': faQuestionCircle,
  'exclamation': faExclamation,
  'exclamation-triangle': faExclamationTriangle,
  'exclamation-circle': faExclamationCircle
};

// Export a function to get icons by name with color
export function getIcon(name, color = null) {
  const iconDef = iconMap[name];

  if (!iconDef) {
    console.warn(`Icon "${name}" not found in icon map`);
    return '';
  }
  //   if (color) {
  //   // Add color class if explicitly specified
  //   classes.push(`text-${color}`);
  // } else {
  //   // Add a default class for CSS targeting when no color specified
  //   classes.push('icon-default');
  // }
  
  // const iconObj = icon(iconDef, { 
  //   classes: classes,
  //   // Remove inline styles completely
  // });
  const iconObj = icon(iconDef, { 
    classes: color ? [`text-${color}`] : [],
    styles: color ? { color } : {}
  });
  
  return iconObj ? iconObj.html[0] : '';
}

// Auto-replace any <i class="fa-..."></i> elements with SVG
export function autoReplace() {
  dom.watch();
}

// Helper to get tree node icons
export function getTreeIcons() {
  return {
    open: getIcon('chevron-down', '#666'),
    close: getIcon('chevron-right', '#666'),
    leaf: getIcon('file-text', '#999'),
    parent: getIcon('folder', '#f8d775')
  };
}
