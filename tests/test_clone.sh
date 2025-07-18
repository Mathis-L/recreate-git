#!/bin/bash
set -e

# --- Variables de couleur ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${YELLOW}🧪 Testing: clone${NC}"

# --- Configuration de l'environnement de test ---
# Utiliser un nom de dossier de test unique pour éviter les conflits
rm -rf tmp_test_clone && mkdir tmp_test_clone && cd tmp_test_clone

# --- Variables de configuration ---
REPO_URL="https://github.com/codecrafters-io/git-sample-3"
YOUR_CLONE_DIR="my_git_clone"
GIT_CLONE_DIR="official_git_clone"

# --- Étape 1: Cloner avec votre programme ---
echo -e "${CYAN}[1/3] Exécution de votre commande 'clone'...${NC}"
../../build.sh clone "$REPO_URL" "$YOUR_CLONE_DIR"

# Vérifier si le répertoire a bien été créé par votre programme
if [ ! -d "$YOUR_CLONE_DIR" ]; then
    echo -e "${RED}[FAIL] Votre programme n'a pas créé le répertoire de destination '${YOUR_CLONE_DIR}'${NC}"
    # Nettoyage avant de quitter
    cd ..
    rm -rf tmp_test_clone
    exit 1
fi
echo -e "${GREEN}Le répertoire '${YOUR_CLONE_DIR}' a été créé.${NC}"


# --- Étape 2: Cloner avec la commande git officielle pour comparaison ---
echo -e "${CYAN}[2/3] Exécution de 'git clone' comme référence...${NC}"
# L'option --quiet permet de garder la sortie du test propre
git clone --quiet "$REPO_URL" "$GIT_CLONE_DIR"


# --- Étape 3: Comparer les répertoires de travail ---
echo -e "${CYAN}[3/3] Comparaison des fichiers extraits...${NC}"
# On compare les arbres de travail, mais pas le contenu du répertoire .git,
# car il peut contenir des différences (timestamps, hooks, logs, etc.).
# 'diff -r' compare les fichiers récursivement.
diff_output=$(diff -r --exclude=".git" "$YOUR_CLONE_DIR" "$GIT_CLONE_DIR" || true)

# Vérifier si la sortie de diff est vide
if [ -z "$diff_output" ]; then
    echo -e "${GREEN}[PASS] Le contenu du dépôt cloné correspond à celui de git. Félicitations ! 🎉${NC}"
else
    echo -e "${RED}[FAIL] Le répertoire de travail créé par votre programme ne correspond pas à celui créé par git.${NC}"
    echo -e "${YELLOW}--- Différences détectées ---${NC}"
    echo "$diff_output"
    echo -e "${YELLOW}--------------------------${NC}"
    # Nettoyage avant de quitter
    cd ..
    rm -rf tmp_test_clone
    exit 1
fi

# --- Nettoyage final ---
cd ..
rm -rf tmp_test_clone

echo ""
echo -e "${GREEN}Le test du clone s'est terminé avec succès.${NC}"